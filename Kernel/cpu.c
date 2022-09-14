/*
 * cpu.c
 *
 *  Created on: Dec 18, 2012
 *      Author: Shaun Yuan
 *      Copyright (c) 2012 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/cpu.h>



struct cpu_state {
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;
};



unsigned long load_cr3(unsigned long cr3)
{
	unsigned long old_cr3;
	asm volatile("movl	%%cr3, %0\n\t"
			"movl	%1,	%%cr3\n\t"
			:"=a"(old_cr3)
			:"d"(cr3));
	return old_cr3;
}
unsigned long get_cr3()
{
	unsigned long cr3 = 0;
	asm volatile ("movl	%%cr3,	%0\n\t"
			:"=a"(cr3)::);
	return cr3;
}
unsigned long get_cr2()
{
	unsigned long cr2 = 0;
	asm volatile ("movl	%%cr2,	%0\n\t"
			:"=a"(cr2)::);

	return cr2;
}






void cpuid(struct cpu_state *cs)
{
	asm volatile (
			"movl	%4,	%%eax\n\t"
			"cpuid\n\t"
			:"=a"(cs->eax), "=b"(cs->ebx),
			 "=c"(cs->ecx), "=d"(cs->edx)
			:"0"(cs->eax));
}

int cpu_has_pse(void)
{
	struct cpu_state cs = {0};
	cs.eax = 1;
	cpuid(&cs);

	return (cs.edx & CPU_PSE);
}
