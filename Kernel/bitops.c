/*
 * bitops.c
 *
 *  Created on: Dec 12, 2012
 *      Author: Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/bitops.h>
#include <kernel/types.h>
/*
 * i use setc(i386+),but gcc change it
 * to setb(fuck!), to get the old bit in CF
 * in linux, which sbb is used,
 * when the old bit is 1, it will
 * return -1 in linux, perhaps a bug...
 * ask linus for more details,
 */

int set_bit(int nr, volatile unsigned long *addr)
{
	int oldbit = 0;
	asm volatile (
			"btsl	%2,	%1\n\t"
			"setc	%0\n\t"
			"clc\n\t"
			:"=m"(oldbit), "=m"(*addr)
			:"ir"(nr)
			:"memory"
			);

	return oldbit;
}

/*
 * return the nr bit state,
 * is nr bit is 1, return 1
 * else return 0
 */

int get_bit(int nr, volatile unsigned long *addr)
{
	int oldbit = 0;
	asm volatile (
			"btl	%2, %1\n\t"
			"setc	%0\n\t"
			"clc\n\t"
			:"=m"(oldbit), "=m"(*addr)
			:"ir"(nr)
			:"memory");
	return oldbit;
}

int __find_first_bit(volatile unsigned long *addr)
{
	int oldbit = -1;
	if (addr == 0)
		return -1;
	asm volatile (
			"bsfl	%1, %0\n\t"
			:"=a"(oldbit)
			:"m"(*addr)
			:"memory");

	return oldbit;
}

int find_first_bit(volatile unsigned long *addr, int offset)
{
	unsigned long *region =(unsigned long *)(addr + offset);

	return __find_first_bit(region);

}

int clear_bit(int nr, volatile unsigned long *addr)
{
	int oldbit = 0;
	asm volatile (
			"btrl	%2, %1\n\t"
			"setc	%0\n\t"
			"clc\n\t"
			:"=m"(oldbit), "=m"(*addr)
			: "ir"(nr)
			:"memory");
	return oldbit;
}



struct kbit_object kbit_obj = {
		.kobj.name = "kbitops",
		.kobj.init = NULL,
		.kobj.destroy = NULL,
		.kobj.next = NULL,
		.find_first_bit = find_first_bit,
		.clear_bit = clear_bit,
		.set_bit = set_bit,
};












