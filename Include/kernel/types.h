/*
 * types.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_TYPES_H_
#define SHAUN_TYPES_H_

typedef unsigned long ulong_t;
typedef unsigned int uint_t;
typedef unsigned short ushort_t;
typedef unsigned char uchar_t;
typedef unsigned long size_t;
typedef long int ssize_t;

typedef unsigned long addr_t;


typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef char s8;
typedef short	s16;
typedef	int 	s32;
typedef	long long	s64;


#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif


typedef unsigned int id_t;
typedef unsigned short mode_t;
typedef unsigned short devno_t;
typedef unsigned int ino_t;
typedef unsigned short nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long off_t;
typedef long int time_t;
typedef long int blksize_t;
typedef long int blkcnt_t;
typedef long int clock_t;
typedef char *	caddr_t;
typedef int pid_t;

typedef unsigned int	n_time;
typedef unsigned int in_addr_t;
typedef unsigned long n_long;
typedef  int socklen_t;





typedef long long int64_t;
typedef unsigned int u_int32_t;
typedef int int32_t;
typedef unsigned long long u_int64_t;



#endif /* SHAUN_TYPES_H_ */
