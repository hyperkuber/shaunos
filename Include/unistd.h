/*
 * unistd.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef UNISTD_H_
#define UNISTD_H_


# define SEEK_SET	0	/* Seek from beginning of file.  */
# define SEEK_CUR	1	/* Seek from current position.  */
# define SEEK_END	2	/* Seek from end of file.  */

void exit(int status);
int write(int fd, void *buf, int size);
int sleep(unsigned long sec);
int open(char *path, int flag, int mode);
#endif /* UNISTD_H_ */
