/*
 * gdt.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/segment.h>
#include <string.h>
#include <kernel/page.h>
#include <asm/io.h>


//segment limit is 20 bits
#define LIMIT_MASK 0xFFFFF

#define KERN_CODE_BASE_ADDR 0x0

#define KERN_DATA_BASE_ADDR 0x0

#define USER_CODE_BASE_ADDR	0x0

#define USER_DATA_BASE_ADDR 0x0


struct segment_descriptor	KGDT[NR_GDTENTRIES];


void init_gdt(void)
{

	struct gdt_ptr	gdtr, *p;
	memset(KGDT, 0, sizeof(KGDT));
	//KGDT[0] is null
	//KGDT[1] is kernel code descriptor
	KGDT[1].base_low = KERN_CODE_BASE_ADDR & 0xFFFFFF;
	KGDT[1].base_high = (KERN_CODE_BASE_ADDR >> 24) & 0xFF;
	KGDT[1].limit_low = LIMIT_MASK & 0xFFFF;
	KGDT[1].limit_high = (LIMIT_MASK >> 16) & 0x0F;

	KGDT[1].granularity = 1;		//in pages
	KGDT[1].avl = 0;
	KGDT[1].db = 1;			//32-bit segment
	KGDT[1].dpl	= 0;		//kernel
	KGDT[1].present = 1;	//in
	KGDT[1].reserved = 0;
	KGDT[1].system = 1;
	KGDT[1].type = 0x0A;	//code segment, read, execute


	//KGDT[2] is kernel data descriptor
	KGDT[2].base_low = KERN_DATA_BASE_ADDR & 0xFFFFFF;
	KGDT[2].base_high = (KERN_DATA_BASE_ADDR >> 24) & 0xFF;
	KGDT[2].limit_low = LIMIT_MASK & 0xFFFF;
	KGDT[2].limit_high = (LIMIT_MASK >> 16) & 0x0F;

	KGDT[2].granularity = 1;		//in pages
	KGDT[2].avl = 0;
	KGDT[2].db = 1;			//32-bit segment
	KGDT[2].dpl	= 0;		//kernel
	KGDT[2].present = 1;	//in
	KGDT[2].reserved = 0;
	KGDT[2].system = 1;
	KGDT[2].type = 0x02;	//data segment, read, write

	//KGDT[3] is tss, initialized in tss.c

	// KGDT[4] is user code segment
	KGDT[4].base_low = USER_CODE_BASE_ADDR & 0xFFFFFF;
	KGDT[4].base_high = (USER_CODE_BASE_ADDR >> 24) & 0xFF;
	KGDT[4].limit_low = LIMIT_MASK & 0xFFFF;
	KGDT[4].limit_high = (LIMIT_MASK >> 16) & 0x0F;

	KGDT[4].granularity = 1;		//in pages
	KGDT[4].avl = 0;
	KGDT[4].db = 1;			//32-bit segment
	KGDT[4].dpl	= 3;		//user
	KGDT[4].present = 1;	//in
	KGDT[4].reserved = 0;
	KGDT[4].system = 1;
	KGDT[4].type = 0x0A;	//code segment, read, execute

	//KGDT[5] is user data segment
	KGDT[5].base_low = USER_DATA_BASE_ADDR & 0xFFFFFF;
	KGDT[5].base_high = (USER_DATA_BASE_ADDR >> 24) & 0xFF;
	KGDT[5].limit_low = LIMIT_MASK & 0xFFFF;
	KGDT[5].limit_high = (LIMIT_MASK >> 16) & 0x0F;

	KGDT[5].granularity = 1;		//in pages
	KGDT[5].avl = 0;
	KGDT[5].db = 1;			//32-bit segment
	KGDT[5].dpl	= 3;		//kernel
	KGDT[5].present = 1;	//in
	KGDT[5].reserved = 0;
	KGDT[5].system = 1;
	KGDT[5].type = 0x02;	//data segment, read, write

	//KGDT[6] is 16bit code segment
	KGDT[6].base_high = 0;
	KGDT[6].base_low = 0;
	KGDT[6].limit_high = 0x0F;
	KGDT[6].limit_low = 0xFFFF;

	KGDT[6].granularity = 1;
	KGDT[6].avl = 0;
	KGDT[6].db = 0;			//16bit segment
	KGDT[6].dpl = 0;
	KGDT[6].present = 1;
	KGDT[6].system = 1;
	KGDT[6].type = 0x0A;

	//KGDT[7] is 16bit data segment
	KGDT[7].base_high = 0;
	KGDT[7].base_low = 0;
	KGDT[7].limit_high = 0x0F;
	KGDT[7].limit_low = 0xFFFF;

	KGDT[7].granularity = 1;
	KGDT[7].avl = 0;
	KGDT[7].db = 0;			//16bit segment
	KGDT[7].dpl = 0;
	KGDT[7].present = 1;
	KGDT[7].system = 1;
	KGDT[7].type = 0x02;



	gdtr.limit = NR_GDTENTRIES * 8 - 1;
	gdtr.addr = PA(KGDT);	//Physical address, changed for int86 real mode
	p = &gdtr;
	lgdt(p);

	asm volatile(
			  "movw		%0, 	%%ax\n\t"
			  "movw		%%ax,	%%ds\n\t"
			  "movw 	%%ax,	%%es\n\t"
			  "movw 	%%ax,	%%fs\n\t"
			  "movw 	%%ax,	%%gs\n\t"
			  "movw 	%%ax,	%%ss\n\t"
			  "ljmp		%1,		$$lable\n\t"
			  "$lable:"
			  ::"i"(KERNEL_DS),"i"(KERNEL_CS):"eax");


}


