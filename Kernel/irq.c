/*
 * irq.c
 *
 *  Created on: Feb 23, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <asm/io.h>
#include <kernel/cpu.h>

#include <kernel/malloc.h>

struct irqaction {
	int irq;
	void *data;
	void (*handler)(int irq, void *data, trap_frame_t *regs);
	struct irqaction *next;
};

struct irq_desc {
	struct irqaction *action;
};

struct _irq {
	unsigned short mask;
	unsigned long base;
};
void (*isr_routine[32])(void) = {0,};


struct irq_desc irq_desc[224] = {{NULL},};

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
#define PIC_EOI		0x20
#define PIC_READ_IRR	0x0a    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR	0x0b    /* OCW3 irq service next CMD read */

static struct _irq _irq = {.mask = 0xFFFF, .base = 0x20};


static unsigned short __pic_get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC1_COMMAND) << 8) | inb(PIC2_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
unsigned short pic_get_irr(void)
{
    return __pic_get_irq_reg(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
unsigned short pic_get_isr(void)
{
    return __pic_get_irq_reg(PIC_READ_ISR);
}


void pic_send_eoi(unsigned char irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);

	outb(PIC1_COMMAND,PIC_EOI);
}


void dispatch_irq(trap_frame_t *regs)
{
	//int v = irq.base + regs->vector;
	int irq = regs->vector;
	struct irq_desc *desc = NULL;
	struct irqaction *act = NULL;
	if (irq > 0x2f || irq < 0x20)
		goto end;
	irq -= _irq.base;
	desc = &irq_desc[irq];
	if ( !desc )
		goto end;

	act = desc->action;
	if (!act)
		goto end;

	do {
		if (act->handler)
			(*act->handler)(irq, act->data, regs);
	} while ((act = act->next) != NULL);


//	if (irq_routine[regs->vector] != NULL){
//		void (*handler)(trap_frame_t *regs);
//		handler = (void *)(irq_routine[regs->vector]);
//		handler(regs);
//	}
end:
	pic_send_eoi(irq);
}

void dispatch_isr(trap_frame_t *regs)
{
	//isr is in 0..31
	if (regs->vector > 0x20 || regs->vector < 0)
		return;
	void (*handler)(trap_frame_t *regs);
	handler =(void *)(isr_routine[regs->vector]);
	handler(regs);
}


int register_irq(int vector, unsigned long handler, void *data, int flag)
{
	//int v = _irq.base + vector;
	struct irq_desc *desc = &irq_desc[vector];
	struct irqaction **p, *old;
	struct irqaction *new = kzalloc(sizeof(struct irqaction));
	if (!new)
		return -ENOMEM;

	new->handler = (void *)handler;
	new->irq = vector;
	new->data = data;


	p = &desc->action;
	if (flag & IRQ_SHARED) {
		while ((old = *p) != NULL)
			p = &(old->next);
	}

	*p = new;

	return 0;

//	if (irq_routine[v] == NULL){
//		irq_routine[v] =(void *)handler;
//		return 0;
//	} else {
//		if (flag & IRQ_REPLACE){
//			irq_routine[v] = (void *)handler;
//			return 0;
//		}
//		return -1;
//	}
}

void bsod(trap_frame_t *regs)
{

	if (regs->vector == 14){
		printk("Oooops, congratulations! you finally crashed the kernel, please mail the information"
				" you see below(only eip and line addr) to shaun@shaunos.com, thanks.\n");
		printk("PAGE FAULT:\nError code:%d, line addr:%x\n", regs->error_code, get_cr2());
		printk("ebp:%x eip:%x", regs->ebp, regs->eip);


	} else
		printk("\nKernel Error\nerror code:%d vector:%d\n", regs->error_code, regs->vector);
	while (1);

}

void enable_irq(int num)
{
	_irq.mask &= ~(1 << num);
	if(num < 8)
		outb(PIC1_DATA, _irq.mask);
	else
	{
		if ( _irq.mask & 4 ) enable_irq(2);
		outb(PIC2_DATA, _irq.mask >> 8);
	}
}

void disable_irq(int num)
{
	_irq.mask |= (1 << num);
	if ( num < 8 )
		outb(PIC1_DATA, _irq.mask);
	else
	{
		if ( (_irq.mask >> 8) == 0xff && !(_irq.mask & 4) ) disable_irq(2);
		outb(PIC2_DATA, _irq.mask >> 8);
	}
}


void spurious_master(trap_frame_t *regs)
{
	//read isr
	unsigned short isr;
	isr = pic_get_isr();

	if (isr & 0x80){
		//this is a real irq7
		pic_send_eoi(0x07);
	} else {
		LOG_INFO("superious irq from master pic\n");
	}
}

void spurious_slave(trap_frame_t *regs)
{
	//read isr
	unsigned short isr;
	isr = pic_get_isr();

	if (isr & 0x8000){
		//this is a real irq15
		pic_send_eoi(15);	//send eoi to slave chip
		pic_send_eoi(0);	//...master chip
	} else {
		LOG_INFO("superious irq from slave pic\n");
		pic_send_eoi(0);		//only send eio to master chip
	}
}


void irq_init()
{
	int i;
	_irq.base = 0;
	for (i=0; i<32; i++)
		isr_routine[i] = (void *)bsod;

	_irq.base = 0x20;

	register_irq(39 - _irq.base, (unsigned long)spurious_master, NULL, IRQ_REPLACE);
	register_irq(47 - _irq.base, (unsigned long)spurious_slave, NULL, IRQ_REPLACE);
}
