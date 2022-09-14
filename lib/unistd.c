/*
 * unistd.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <kernel/syscall.h>
//typedef	int (*syscall)(trap_frame_t *regs);

void exit(int status)
{
	while (1){
		asm ("int $0x80\n\t"
				::"a"(nr_sys_exit),"b"(status));
	}

}

int open(char *path, int flag, int mode)
{
	int ret;
	__asm__ __volatile__("int $0x80\n\t"
			:"=a"(ret):"0" (nr_sys_open),"b"(path),"c"(flag),"d"(mode));

	return ret;
}

int write(int fd, void *buf, int size)
{
	int ret;
	__asm__ __volatile__("int $0x80\n\t"
			:"=a"(ret):"0" (nr_sys_write),"b"(fd),"c"((long)(buf)),"d"(size));

	return ret;
}


int sleep(unsigned long sec)
{
	int ret;
	__asm__ __volatile__("int $0x80\n\t"
			:"=a"(ret):"0"(nr_sys_sleep), "b"(sec));

	return ret;
}
