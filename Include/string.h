/*
 * string.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_STRING_H_
#define SHAUN_STRING_H_

#include "kernel/types.h"


void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, ssize_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strdup(const char *s);
void *memmove(void *dest, const void *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
int strnlen_user(const char *path, int size);
int strlen_user(char *path);

#define bcopy(s, d, n)	memcpy((d), (s), (n))
#define bzero(s, n)	memset((s), 0, (n))


//this should be in stdio.h
int sprintf (char *str, const  char  *format,...);
#endif /* SHAUN_STRING_H_ */
