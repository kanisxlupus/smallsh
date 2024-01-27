#!/usr/bin/env bash

if [ $# -ne 1 ]
then
  printf "Usage: $0 /path/to/smallsh\n" >&2
  exit 1
fi
export TMPDIR=/scratch
readonly workdir=`mktemp -d`
readonly homedir=`mktemp -d`
readonly outdir=`mktemp -d`

trap 'rm -rf "$workdir" "$homedir" "$outdir";' EXIT 

hr=$(perl -E 'say "\xe2\x80\x95" x 120' 2>/dev/null)

randval=$((RANDOM % 254+1))

cp "$1" "$workdir/smallsh"
cd "$workdir"

printf 'Beginning Test Script!\n'

timeout 5 env - PS1='' HOME="$homedir" setsid bash <<__OUTEREOF__ &
export IFS=$'\t'$'\n'
exec ./smallsh << __EOF__
echo	$hr
printf	$(tput bold setaf 4)Variable expansions$(tput sgr0)\n
printf	Reported expansion of %c%c: "%s" (Expecting "%s", $(tput setaf 4 bold)10 points$(tput sgr0))\n	$	$	\\\$$	\$$
printf	Reported expansion of %c%c: "%s" (Expecting "%s", $(tput setaf 4 bold)5 points$(tput sgr0))\n	$	?	\\\$?	0
printf	Reported expansion of %c%c: "%s" (Expecting "", $(tput setaf 4 bold)10 points$(tput sgr0))\n	$	!	\\\$!
printf	Reported expansion of %c%c: "%s" (Expecting "$homedir/", $(tput setaf 4 bold)10 points$(tput sgr0))\n	~	/	~/

printf	\nTesting: $(tput bold)bash -c 'exit %d'$(tput sgr0)	$randval
bash	-c	exit $randval
printf	\nReported expansion of %c%c: "%s" (Expecting "%s", $(tput setaf 4 bold)5 points$(tput sgr0))\n	$	?	\\\$?	$randval
printf	\nTesting: $(tput bold)echo PID=%c%c. Status=%c%c. There's no place like %c%cHome!$(tput sgr0) ($(tput setaf 4 bold)10 points$(tput sgr0))\n	\$	\$	\$	?	~	/
bash	-c	exit $randval
echo	PID=\\\$$. Status=\\\$?. There's no place like	~/Home!

echo	$hr
printf	$(tput bold setaf 4)Parsing Comments$(tput sgr0)\n
printf	Testing: $(tput bold)echo Success! # ...not! This shouldn't be printed!$(tput sgr0) $(tput setaf 4 bold)5 points$(tput sgr0))\n
echo	Success!	#	...not! This shouldn't be printed!

echo	$hr
printf	$(tput bold setaf 4)Testing built-in cd command$(tput sgr0)\n
printf	Testing: $(tput bold)pwd$(tput sgr0)\n
pwd
printf	Testing: $(tput bold)cd$(tput sgr0)\n
cd
printf	Testing: $(tput bold)pwd$(tput sgr0) (Expecting "%s", $(tput setaf 4 bold)15 points$(tput sgr0))\n	$homedir
pwd
printf	Testing: $(tput bold)mkdir pid-%c%c-dir$(tput sgr0)\n	$	$
mkdir	pid-\\\$\\\$-dir
printf	Testing: $(tput bold)ls$(tput sgr0) (Expecting "pid-\$\$-dir", $(tput setaf 4 bold)10 points$(tput sgr0))\n
ls
printf	Testing: $(tput bold)cd pid-\$\$-dir$(tput sgr0)\n
cd	pid-\\\$\\\$-dir
printf	Testing: $(tput bold)pwd$(tput sgr0) (Expecting "%s", $(tput setaf 4 bold)10 points$(tput sgr0))\n	$homedir/pid-\$\$-dir
pwd

echo	$hr
printf	$(tput bold setaf 4)Testing built-in exit command$(tput sgr0)\n
printf	Testing: $(tput bold)exit $randval$(tput sgr0)\n
exit	$randval
__EOF__
__OUTEREOF__
bgpid=$!
wait $!
stat=$?
echo $hr
printf "Smallsh exited with value: $stat (expected $randval, $(tput setaf 4 bold)15 points$(tput sgr0))\n"
sleep 1
printf "Checking for zombies: ($(tput setaf 4 bold)-5 points$(tput sgr0) if detected)\n"
pgrep -af -s $! || echo none detected!
pkill -9 -s $!

printf "\n\nStarting next set of tests!\n"

timeout 10 env - PS1='' HOME="$homedir" setsid bash <<__OUTEREOF__ &
export IFS=$'\t'$'\n'
exec ./smallsh << __EOF__
echo	$hr
printf	$(tput bold setaf 4)Input / Output redirection operators$(tput sgr0)\n
printf	Testing: $(tput bold)head -c 1000 /dev/zero > 1k-random-bytes$(tput sgr0)\n
head	-c	1000	/dev/zero	>	1k-random-bytes
printf	Testing: $(tput bold)wc -c 1k-random-bytes$(tput sgr0) (Expecting "1000 1k-random-bytes", $(tput setaf 4 bold)15 points$(tput sgr0))\n
wc	-c	1k-random-bytes
printf	Testing: $(tput bold)wc -c < 1k-random-bytes$(tput sgr0) (Expecting "1000", $(tput setaf 4 bold)10 points$(tput sgr0))\n
wc	-c	<	1k-random-bytes
printf	Testing: $(tput bold)head -c 10000 < /dev/zero > 10k-random-bytes$(tput sgr0)\n
head	-c	10000	<	/dev/zero	>	10k-random-bytes
printf	Testing: $(tput bold)wc -c 10k-random-bytes$(tput sgr0) (Expecting "10000 10k-random-bytes", $(tput setaf 4 bold)15 points$(tput sgr0))\n
wc	-c	10k-random-bytes

echo	$hr
printf	$(tput bold setaf 4)Background Commands$(tput sgr0)\n
sleep	1	&
printf	Testing: $(tput bold)sleep 1 &$(tput sgr0) (Expecting "Child process \\\$! done. Exit status 0.", $(tput setaf 4 bold)15 points$(tput sgr0) for pid, $(tput setaf 4 bold)5 points$(tput sgr0) for status.)\n
sleep	2

sleep	100	&
printf	$(tput bold)Sending SIGSTOP to a sleeping proccess$(tput sgr0) (Expecting "Child process \\\$! stopped. Continuing.", $(tput setaf 4 bold)5 points$(tput sgr0))\n
kill	-SIGSTOP	\\\$!
sleep	1

sleep	100	&
printf	$(tput bold)Sending SIGINT to a background process$(tput sgr0) (Expecting "Child process \\\$! done. Signaled 2.", $(tput setaf 4 bold)10 points$(tput sgr0))\n
kill	-SIGINT	\\\$!
sleep	1
__EOF__
__OUTEREOF__

wait $!
stat=$?
echo $hr
sleep 1
printf "Checking for zombies: ($(tput setaf 4 bold)-5 points$(tput sgr0) if detected)\n"
pgrep -af -s $! || echo none detected!
pkill -9 -s $!
