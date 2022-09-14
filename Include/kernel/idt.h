/*
 * idt.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_IDT_H_
#define SHAUN_IDT_H_
#include <kernel/types.h>
#include <kernel/kernel.h>
#define IDT_BASE	0x0000
#define NR_IDT_ENTRIES	256

#define SYSCALL	0x80


struct idt_desc {
	ushort_t addlow packed;
	ushort_t segselector packed;
	uint_t	reserved:8 packed;
	uint_t	type:5 packed;
	uint_t	dpl:2 packed;
	uint_t	present:1 packed;
	ushort_t	addrhigh packed;
} __attribute__((__packed__));

struct idt_ptr {
   u16 limit;
   u32 base;
}__attribute__((__packed__));

typedef struct interrupt_state {
	uint_t gs;
	uint_t fs;
	uint_t ds;
	uint_t es;
	uint_t ebp;
	uint_t edi;
	uint_t esi;
	uint_t edx;
	uint_t ecx;
	uint_t ebx;
	uint_t eax;
	uint_t	vector;
	uint_t error_code;
	uint_t eip;
	uint_t cs;
	uint_t eflags;
	uint_t esp;
	uint_t ss;

} trap_frame_t;


void idt_init(void);




#endif /* SHAUN_IDT_H_ */
