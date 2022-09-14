/*
 * malloc.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/malloc.h>
#include <kernel/bget.h>
#include <kernel/kernel.h>
#include <kernel/assert.h>
#include <string.h>
#include <kernel/mm.h>


void heap_init(unsigned long start, unsigned long size)
{
	bpool((void *)start, size);
}


void *kmalloc(unsigned long size)
{
	void *result;
	u32 newpool;
	int x;
	assert(size > 0);
again:
	local_irq_save(&x);
	result = bget(size);
	local_irq_restore(x);

	if (result == NULL) {
		//we alloc 4M memory when the pool needs group
		newpool = alloc_pages(ALLOC_NORMAL|ALLOC_ZEROED, 10);
		if (newpool == 0)
			return NULL;
		bpool((void *)newpool, 4<<20);
		goto again;
	}


	return result;
}

void kfree(void *buf)
{
	int x;
	local_irq_save(&x);
	brel(buf);
	local_irq_restore(x);
}


void *kzalloc(unsigned long size)
{
	void *ret = kmalloc(size);
	if (ret){
		memset(ret, 0, size);
	}

	return ret;
}
