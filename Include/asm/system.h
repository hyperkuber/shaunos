/*
 * system.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_SYSTEM_H_
#define SHAUN_SYSTEM_H_
#include "kernel/types.h"

static inline u16 readaddr16(addr_t seg,addr_t addr)
{
	u16 v = 0;
	u32 fs;
	fs = seg;
	asm volatile("movw %%fs:%1,%0" : "=r" (v) : "m" (*(u16 *)addr));
	return v;
}

static inline u8 readaddr8(addr_t seg,addr_t addr)
{
	u8 v = 0;
	u32 fs;
	fs = seg;
	asm volatile("movb %%fs:%1,%0" : "=r" (v) : "m" (*(u8 *)addr));
	return v;
}

static inline u32 readaddr32(addr_t seg, addr_t addr)
{
	u32 v = 0;
	u32 fs;
	fs = seg;
	asm volatile("movl %%fs:%1,%0" : "=r" (v) : "m" (*(u32 *)addr));
	return v;
}

static inline u32 iomem_readl(const volatile void *addr)
{
	return *(volatile u32 *)addr;
}


static inline u8 iomem_readb(const volatile void *addr)
{
	return *(volatile u8 *)addr;
}

static inline u16 iomem_readw(const volatile void *addr)
{
	return *(volatile u16 *)addr;
}

static inline void iomem_writeb(volatile void *addr, u8 val)
{
	*(volatile u8 *)addr = val;
}

static inline void iomem_writew(volatile void *addr, u16 val)
{
	*(volatile u16 *)addr = val;
}

static inline void iomem_writel(volatile void *addr, u32 val)
{
	*(volatile u32 *)addr = val;
}



#endif /* SHAUN_SYSTEM_H_ */
