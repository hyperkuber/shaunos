/*
 * tss.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/kernel.h>
#include <kernel/tss.h>
#include <kernel/segment.h>
#include <kernel/mm.h>

struct tss ktss;

extern struct segment_descriptor	KGDT[NR_GDTENTRIES];

#define GDT_TSS	3

void tss_init(void)
{
	ushort_t selector = 0;
	KGDT[GDT_TSS].type = 0x09;
	KGDT[GDT_TSS].system = 0;
	KGDT[GDT_TSS].dpl = 0;
	KGDT[GDT_TSS].db = 0;
	KGDT[GDT_TSS].reserved = 0;
	KGDT[GDT_TSS].present = 1;
	KGDT[GDT_TSS].granularity = 0;
	KGDT[GDT_TSS].avl = 0;
	KGDT[GDT_TSS].limit_low = sizeof(struct tss) & 0xFFFF;
	KGDT[GDT_TSS].limit_high = (sizeof(struct tss) >> 16) &0x0F;
	KGDT[GDT_TSS].base_high = (((ulong_t)(struct tss *)&ktss) >> 24 ) & 0xFF;
	KGDT[GDT_TSS].base_low = ((ulong_t)(struct tss *)&ktss) & 0xFFFFFF;


	ktss.ss0 = KERNEL_DS;
	//ktss.esp0 = KERNEL_STACK + 4096;
	selector = (0 & 0x03) | (0 << 2) | ((GDT_TSS & 0x1FFF) << 3);
	__asm__ volatile ("ltr (%0)\n\t"
			::"r"(&selector));


}


void set_kernel_stack(ulong_t esp0)
{
	ktss.ss0 = KERNEL_DS;
	ktss.esp0 = esp0;


}

