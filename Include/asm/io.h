/*
 * io.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_IO_H_
#define SHAUN_IO_H_

#include <kernel/types.h>
#include <kernel/idt.h>
#include <kernel/segment.h>
//#define outb(value,port) __asm__ __volatile__ ("out %%al,%%dx;"::"d"(port),"a"(value))
//#define inb(port) ({unsigned char value;__asm__ __volatile__ ("in %%dx,%%al;":"=a"(value):"d"(port));value;})

//#define outw(value,port) __asm__ __volatile__ ("out %%ax,%%dx;"::"d"(port),"a"(value))
//#define inw(port) ({unsigned short int value;__asm__ __volatile__ ("in %%dx,%%ax;":"=a"(value):"d"(port));value;})

#define nop	{__asm__ __volatile__("nop":::);}

static inline void outb(ushort_t port, uchar_t value)
{
	__asm__ __volatile__("outb %b0, %w1"::"a"(value), "d"(port));
}


static inline uchar_t inb(ushort_t port)
{
	uchar_t value;
	__asm__ __volatile__("inb %w1, %b0":"=a"(value):"d"(port));
	return value;
}

static inline void outw(ushort_t port, ushort_t value)
{
	__asm__ __volatile__("outw %w0, %w1"::"a"(value),"d"(port));
}


static inline ushort_t inw(ushort_t port)
{
	ushort_t value;
	__asm__ __volatile__ ("inw %w1, %w0":"=a"(value):"d"(port));
	return value;
}

static inline void outl(ushort_t port, uint_t value)
{
	asm volatile ("outl %0, %1"::"a"(value), "d"(port));
}

static inline uint_t inl(ushort_t port)
{
	uint_t value;
	__asm__ __volatile__ ("inl %1, %0":"=a"(value):"d"(port));
	return value;
}



static __inline void
lidt(struct idt_ptr *idtr)
{
	__asm __volatile("lidt (%0)"::"r" (idtr));
}

static __inline void
sidt(struct idt_ptr *idtr)
{
	__asm __volatile("sidt (%0)"::"r" (idtr):"memory");
}

static __inline void __attribute__ ((always_inline))
lgdt(struct gdt_ptr *gdtr)
{
	__asm __volatile("lgdt (%0)"::"r" (gdtr));
}




#endif /* SHAUN_IO_H_ */
