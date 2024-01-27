#ifndef STR_SUB
#define STR_SUB

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);
char *str_frontSub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);

#endif
