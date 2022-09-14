/*
 * tss.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#ifndef TSS_H_
#define TSS_H_
#include <kernel/types.h>
#include <kernel/kernel.h>
struct tss {
	u16 	backlink;
	u16 	rsvd1;
	u32		esp0;
	u16 	ss0;
	u16 	rsvd2;
	u32		esp1;
	u16		ss1;
	u16		rsvd3;
	u32		esp2;
	u16		ss2;
	u16		rsvd4;
	u32		cr3;
	u32		eip;
	u32		eflags;
	u32		eax;
	u32		ecx;
	u32		edx;
	u32		ebx;
	u32		esp;
	u32		ebp;
	u32		esi;
	u32		edi;

	u16		es;
	u16		rsvd5;
	u16		cs;
	u16		rsvd6;
	u16		ss;
	u16		rsvd7;
	u16		ds;
	u16		rsvd8;
	u16		fs;
	u16		rsvd9;
	u16		gs;
	u16		rsvd10;

	u16		ldt;
	u16		rsvd11;

	u32		t:1;
	u32		rsvd12:15;
	u16		iomap_base;

}packed;

void tss_init(void);

void set_kernel_stack(ulong_t esp0);

#endif /* TSS_H_ */
