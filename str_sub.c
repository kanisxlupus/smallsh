/*#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <stdio.h>

//simple search and replace
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);

#include "str_sub.h"
int main(int argc, char *argv[])
{
  if (argc != 4) exit(1);
  char *line = NULL;
  size_t n = 0;

  printf("str_gsub\n");
  getline(&line, &n, stdin);
  {
    char *ret = str_gsub(&line, argv[1], argv[2]);
    if (!ret) exit(1);
    line = ret;
  }
  printf("%s\n", line);
  //free(line);

  printf("str_frontSub\n");
  getline(&line, &n, stdin);
  {
    char *ret = str_frontSub(&line, "~/", argv[3]);
    if (!ret) exit(1);
    line = ret;
  }
  printf("%s\n", line);
  free(line);

  return 0;
}
*/

#include "str_sub.h"

// Global search and replace on haystack
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub)
{
  char *str = *haystack;
  size_t haystackLen = strlen(str);
  size_t const needleLen = strlen(needle);
  size_t subLen = strlen(sub);

  for(;(str = strstr(str, needle));)
  {
    ptrdiff_t off = str - *haystack;
    if (subLen > needleLen)
    {
      str = realloc(*haystack, sizeof **haystack * (haystackLen + subLen - needleLen + 1));
      
      if (!str) goto exit;

      *haystack = str;
      str = *haystack + off;
    }

    memmove(str + subLen, str + needleLen, haystackLen + 1 - off - needleLen);
    memcpy(str, sub, subLen);
    haystackLen = haystackLen + subLen - needleLen;
    str += subLen;

  }

  str = *haystack;

  if (subLen < needleLen)
  {
    str = realloc(*haystack, sizeof **haystack * (haystackLen + 1));
    if (!str) goto exit;

    *haystack = str;
  }

exit:  
  return str;
}

// Search for needle and replace all but last char of needle at beginning of haystack
char *str_frontSub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub)
{
  char *str = NULL;
  size_t haystackLen = strlen(*haystack);
  size_t const needleLen = strlen(needle);
  size_t subLen = strlen(sub);

  char *haystackSubstr = NULL;

  //get the first subLen chars from haystack
  haystackSubstr = malloc(sizeof *needle * needleLen);
  memcpy(haystackSubstr, *haystack, needleLen);
  //printf("needleLen: %zu\nneedle: %s\n", needleLen, needle);
  //printf("Haystack Substr: %s\n", haystackSubstr);

  if (strcmp(haystackSubstr, needle) == 0)
  {
    str = *haystack;
    if (subLen > needleLen)
    {
      str = realloc(*haystack, sizeof **haystack * (haystackLen + subLen - needleLen + 1));
      
      if (!str) goto exit;

      *haystack = str;
    }

    memmove(str + subLen, str + needleLen - 1, haystackLen + 1 - needleLen);
    memcpy(str, sub, subLen);
  
    if (subLen < needleLen)
    {
      str = realloc(*haystack, sizeof **haystack * (haystackLen + 1));
      if (!str) goto exit;
      *haystack = str;
    }
  }
  
  str = *haystack;
  
exit:
  //printf("Returning: %s\n", str);
  return str;  
}
