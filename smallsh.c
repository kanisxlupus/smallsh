#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "str_sub.h"

void handle_SIGINT(int signo)
{
  //printf("SIGINT handler being used. SIGINT not ignored.\n");
  return;
}

int main(void)
{
  errno = 0;

  // Variables
  char *prompt = getenv("PS1");
  char *line = NULL;
  size_t lineBytes = 0;
  char *delim = getenv("IFS");
  char *home = getenv("HOME");
  pid_t smallshPid = getpid(); //$$
  char *pidStr = malloc(sizeof smallshPid);
  int fgExit = 0; //$?
  char* fgExitStr = malloc(sizeof (int));
  pid_t childPid = 0; //$!
  char* childStr = malloc(sizeof smallshPid);
  pid_t spawnPid = -5;
  long int exitArg;

  struct sigaction SIGINT_action = {0};
  struct sigaction SIGTSTP_action = {0};
  struct sigaction SIGINT_old = {0};
  struct sigaction SIGTSTP_old = {0};
  
  // Set prompt appropriately
  if (prompt == NULL)
  {
    prompt = "";
  }

  // Set token delimiter appropriately
  if (delim == NULL)
  {
    delim = " \t\n";
  }

  // Cast PID to String
  if (sprintf(pidStr, "%jd",(intmax_t) smallshPid) < 0)
  {
    err(1, "sprintf");
  }
  
  // fill sigaction structs
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  sigaction(SIGINT, &SIGINT_action, &SIGINT_old);

  SIGTSTP_action.sa_handler = SIG_IGN;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;

  sigaction(SIGTSTP, &SIGTSTP_action, &SIGTSTP_old);

  /* LOOP FOREVER */
  for (;;)
  {
    /* STEP 1: INPUT */
  _input:;
    // Check for un-waited-for background processes in the same PGID
    int childStatus = -5;
    pid_t bgChildPid = waitpid(0, &childStatus, WNOHANG | WUNTRACED);
    while (bgChildPid > 0 && childPid)
    {
      if (WIFEXITED(childStatus))
      {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bgChildPid, WEXITSTATUS(childStatus));
      }
 
      else if (WIFSTOPPED(childStatus))
      {
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) bgChildPid);
        kill(bgChildPid, SIGCONT);
      }
      else
      {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) bgChildPid, WTERMSIG(childStatus));
      }

      bgChildPid = waitpid(0, &childStatus, WNOHANG | WUNTRACED);
    }

    errno = 0;

    //Print Prompt
    fprintf(stderr, "%s", prompt);

    SIGINT_action.sa_handler = handle_SIGINT;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Reading a line of input
    ssize_t lineLength = getline(&line, &lineBytes, stdin);
   
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);

    if (feof(stdin) != 0)
    {
      goto _implied_exit;
    }

    // Error Handling
    if (lineLength == -1)
    {
      if (errno == EINTR)
      {
        clearerr(stdin);
        fprintf(stderr, "\n\n");
      }
      else
      {
        err(errno, "stdin");
      }
    }


    if (strcmp(line, "\n") == 0)
    {
      goto _input;
    }

    /*STEP 2: WORD SPLITTING */
    char* tokens[512];
    char* token = strtok(line, delim);
    int numTokens = 0;

    while(token != NULL)
    {
      tokens[numTokens] = strdup(token);
      ++numTokens;
      token = strtok(NULL, delim);
    }
    
    /*STEP 3: EXPANSION */
    for (int i = 0; i < numTokens; i++)
    {
      //replace ~/ with HOME
      str_frontSub(&tokens[i], "~/", home);

      //replace $$ with smallsh PID
      str_gsub(&tokens[i], "$$", pidStr);

      //replace $? with exit status of last foreground command
      if (sprintf(fgExitStr, "%d", fgExit) < 0)
      {
        err(1, "fgExit -> fgExitStr");
      }

      str_gsub(&tokens[i], "$?", fgExitStr);

      childStr = malloc(sizeof childPid);
      
      //replace $! with pid of most recent background process
      if (childPid == 0)
      {
        childStr = "";
      }
      else if (sprintf(childStr, "%d", childPid) < 0)
      {
        err(1, "childPid -> childStr");
      }

      str_gsub(&tokens[i], "$!", childStr);
    }

    /*STEP 4: PARSING */
    // Get rid of comments
    for (int i = 0; i < numTokens; i++)
    {
      if (strcmp(tokens[i], "#") == 0)
      {
        for (int j = i; j < numTokens; j++)
        {
          tokens[j] = NULL;
          free(tokens[j]);
        }

        numTokens -= numTokens - i;
      }
    }

    if (numTokens == 0) goto _input;

    // Check last word for '&'
    bool runInBg = false;

    if (strcmp(tokens[numTokens - 1], "&") == 0)
    {
      tokens[numTokens - 1] = NULL;
      free(tokens[numTokens - 1]);
      runInBg = true;
      --numTokens;
    }

    if (numTokens == 0) goto _input;

    // Check if (potentially new) last word is preceded by '<' or '>' TWICE
    char *infile = NULL;
    char *outfile = NULL;

    for (int i = 0; i < 2; i++)
    {
      if (numTokens > 1 && strcmp(tokens[numTokens - 2], "<") == 0)
      {
        infile = strdup(tokens[numTokens - 1]);
        tokens[numTokens - 2] = NULL;
        tokens[numTokens - 1] = NULL;
        free(tokens[numTokens - 2]);
        free(tokens[numTokens - 1]);
        numTokens -= 2;
      }
      else if (numTokens > 1 && strcmp(tokens[numTokens - 2], ">") == 0)
      {
        outfile = strdup(tokens[numTokens - 1]);
        tokens[numTokens - 2] = NULL;
        tokens[numTokens - 1] = NULL;
        free(tokens[numTokens - 2]);
        free(tokens[numTokens - 1]);
        numTokens -= 2;
      }
    }

    /*STEP 5: EXECUTION */
    // Check for command word, return to STEP 1 if not present
    if (numTokens == 0) goto _input;

    // EXIT built in command
    if (strcmp(tokens[0], "exit") == 0)
    {
      if (numTokens > 2)
      {
        // PRINT ERROR
        fprintf(stderr, "Error: Invalid number of arguments\n");
        // SET EXIT STATUS TO NONZERO
        fgExit = -1;

        goto _input;
      }

      if (numTokens == 2)
      {
        exitArg = strtol(tokens[1], NULL, 0);
        if (errno != 0)
        {
          // PRINT ERROR
          fprintf(stderr, "Error: Invalid argument\n");
          
          // SET EXIT STATUS TO NONZERO
          fgExit = -1;

          goto _input;
        }
      }
      else
      {
      _implied_exit:
        exitArg = strtol(fgExitStr, NULL, 0);

        // ERROR HANDLING FOR STRTOL()
      }

      fprintf(stderr, "\nexit\n");
      
      // Free malloc'd pointers
      pidStr = NULL;
      childStr = NULL;
      fgExitStr = NULL;

      free(pidStr);
      free(childStr);
      free(fgExitStr);

      kill(0, SIGINT);
      exit(exitArg);

      err(errno, "exit"); 
    }

    // CD built in command
    else if (strcmp(tokens[0], "cd") == 0)
    {
      if (numTokens > 2)
      {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        fgExit = -1;

        goto _input;
      }

      char *newDir;

      if (numTokens == 2)
      {
        newDir = tokens[1];
      }
      else
      {
        newDir = home;
      }

      if (chdir(newDir) != 0)
      {
        errno = 0;
        fprintf(stderr, "Error: Unable to change working directory to %s\n", newDir, errno);
        fgExit = -1;
        goto _input;
      }
    }

    // Execute non-built-in commands to child processes
    else
    {
      spawnPid = fork();

      switch (spawnPid)
      {
        case -1:
        {
          errno = 0;
          fprintf(stderr, "Error: Unable to fork child process\n");
          fgExit = -1;
          goto _input;
        }        
        case 0:
        {
          int inFd = -5;
          int outFd = -5;
   
          // Child process behavior after fork
          // All signals reset to their original dispositions when smallsh was invoked
          sigaction(SIGINT, &SIGINT_old, NULL); 
          sigaction(SIGTSTP, &SIGTSTP_old, NULL);

          // Open inputFile for reading if specified
          if (infile)
          {
            inFd = open(infile, O_RDONLY);
            if (inFd == -1)
            {
              fprintf(stderr, "Error: Unable to open %s for reading\n", infile);
              exit(1);
            }

            if (dup2(inFd, 0) == -1)
            {
              fprintf(stderr, "Error: Unable to redirect stdin to %s\n", infile);
              exit(2);
            }
          }
          // Open outputFile for writing if specified
          if (outfile)
          {
            outFd = open(outfile, O_WRONLY | O_CREAT, 0777);
            if (outFd == -1)
            {
              fprintf(stderr, "Error: Unable to open/create %s for writing\n.", outfile);
              exit(1);
            }

            if (dup2(outFd, 1) == -1)
            {
              fprintf(stderr, "Error: Unable to redirect stdout to %s\n", outfile);
              exit(2);
            }
          }
          // Exec
          tokens[numTokens] = NULL;

          execvp(tokens[0], tokens);

          // Throw error if exec fails
          fprintf(stderr, "Error: execvp() failed on %s\n", tokens[0]);
          exit(1);
          break;
        }
        default:
          // Smallsh behavior after fork
          if (runInBg)
          {
            //fprintf(stderr, "childPid being set to %jd\n", (intmax_t) spawnPid);
            childPid = spawnPid;
          }
          else
          {
            /* STEP 6: WAITING */
            // Blocking wait on fg child process. Set fgExit to exit status of waited for command
            int fgStatus = 0;

            waitpid(spawnPid, &fgStatus, WUNTRACED);

            if (WIFEXITED(fgStatus))
            {
              fgExit = WEXITSTATUS(fgStatus);
            }
            // Check for termination by signal and set fgExit to 128 + [signal number]
            else           
            {
              fgExit = 128 + WTERMSIG(fgStatus);
            
              if (WIFSTOPPED(fgStatus))
              {
                fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnPid);
                kill(spawnPid, SIGCONT);
                childPid = spawnPid;    
              }
            }
          }

          break;
        }
      }
  
    /* FREE EVERYTHING THAT NEEDS TO BE FREED */
    for (int i = 0; i < numTokens; i++)
    {
      tokens[i] = NULL;
      free(tokens[i]);
    }
    
    if (infile) free(infile);
    if (outfile) free(outfile);
  }

  return 0;
}
