/*
 * int86.c
 *
 *  Created on: Feb 18, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <string.h>
#include <kernel/idt.h>
#include <asm/io.h>
#include <kernel/timer.h>
#include <kernel/int86.h>


static unsigned char int86_trampoline[] = {
#include "int86_tramp.h"
};


static struct idt_ptr idt16;

unsigned short int86_irq_mask;

#define INT86_OPCODE	((unsigned long *)0x1060)
#define INT86_CALL		((void (*)()) 0x1000)

int int86_init()
{
	memcpy((void *)0x1000, int86_trampoline, sizeof(int86_trampoline));

	idt16.base = 0;
	idt16.limit = 4*256 -1;

	int86_irq_mask = 0xFFFF;
	return 0;
}


void int86(int intnum, int86_regs_t *inregs, int86_regs_t *outregs)
{
	struct idt_ptr	idt_saved;
	int x = 0;
	unsigned short irqmask;
	int86_regs_t *int86_regs = (int86_regs_t *)0x1fec;
	local_irq_save(&x);

	if (inregs != NULL){
		*int86_regs = *inregs;
	} else{
		memset((void *)int86_regs, 0, sizeof(int86_regs_t));
	}

	INT86_OPCODE[0] = 0x909000cd | (intnum & 0xff) << 8;
	INT86_OPCODE[1] = 0x90909090;

	//save current irq mask
	irqmask = inb(PIC1_MASK);
	irqmask |= inb(PIC2_MASK) << 8;
	//set bios irq mask
	outb(PIC1_MASK, int86_irq_mask & 0xFF);
	outb(PIC2_MASK, int86_irq_mask >> 8);
	//set bios idt
	sidt(&idt_saved);
	lidt(&idt16);
	//jmp to 0x1000
	INT86_CALL();

	lidt(&idt_saved);

	outb(PIC1_MASK, irqmask & 0xFF);
	outb(PIC2_MASK, irqmask >> 8);

	if (outregs != NULL)
		*outregs = *((int86_regs_t *)0x1fec);

	local_irq_restore(x);
}

