/*
 * syscall.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SYSCALL_H_
#define SYSCALL_H_

#include <kernel/types.h>


struct sendto_args {
	int fd;
	caddr_t buf;
	size_t len;
	int flags;
	caddr_t to;
	int tolen;
};

struct recvfrom_args {
	int fd;
	caddr_t buf;
	size_t len;
	int flags;
	caddr_t from;
	int *fromlen;
};

struct connect_args {
	int fd;
	caddr_t buf;
	size_t len;
};


struct socketpair_args {
	int domain;
	int type;
	int protocol;
	int *sv;
};

enum {
	nr_sys_exit = 0,
	nr_sys_open,
	nr_sys_read,
	nr_sys_write,
	nr_sys_unlink,
	nr_sys_close,
	nr_sys_sleep,
	nr_sys_wait,
	nr_sys_sbrk,
	nr_sys_fstat,
	nr_sys_fork,
	nr_sys_getpid,
	nr_sys_getppid,
	nr_sys_isatty,
	nr_sys_kill,
	nr_sys_lseek,
	nr_sys_stat,
	nr_sys_times,
	nr_sys_execve,
	nr_sys_gettimeofday,
	nr_sys_socket,
	nr_sys_bind,
	nr_sys_listen,
	nr_sys_accept,
	nr_sys_sendto,
	nr_sys_sendmsg,
	nr_sys_recvfrom,
	nr_sys_recvmsg,
	nr_sys_shutdown,
	nr_sys_ioctl,
	nr_sys_fcntl,
	nr_sys_getdents,
	nr_sys_setloglevel,
	nr_sys_dup2,
	nr_sys_chdir,
	nr_sys_fchdir,
	nr_sys_getuid,
	nr_sys_geteuid,
	nr_sys_getgid,
	nr_sys_getegid,
	nr_sys_setpgid,
	nr_sys_getpgid,
	nr_sys_getpgrp,
	nr_sys_setpgrp,
	nr_sys_tcgetpgrp,
	nr_sys_tcsetpgrp,
	nr_sys_mkdir,
	nr_sys_frame_create,
	nr_sys_frame_text,
	nr_sys_frame_show,
	nr_sys_resolve,
	nr_sys_connect,
	nr_sys_readv,
	nr_sys_writev,
	nr_sys_getpriority,
	nr_sys_setpriority,
	nr_sys_getrusage,
	nr_sys_setitimer,
	nr_sys_getitimer,
	nr_sys_vfork,
	nr_sys_select,
	nr_sys_getlogin,
	nr_sys_socketpair,
	nr_sys_seteuid,
	nr_sys_setegid,
	nr_sys_getdirentries,
	nr_sys_fstatfs,
	nr_sys_statfs,
	nr_sys_mmap,
	nr_sys_munmap,
	nr_sys_utimes,
	nr_sys_wait4,
	nr_sys_getrlimit,
	nr_sys_setrlimit,
	nr_sys_ftruncate,
	nr_sys_fsync,
	nr_sys_settimeofday,
	nr_sys_getsockopt,
	nr_sys_setsockopt,
	nr_sys_chmod,
	nr_sys_fchmod,
	nr_sys_window_create,
	nr_sys_get_message,
	nr_sys_frame_draw_char
};

#endif /* SYSCALL_H_ */


