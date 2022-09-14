/*
 * mm.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_MM_H_
#define SHAUN_MM_H_

#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/page.h>
#define HIGHMEM_START 0x100000

#define KERNEL_STACK (HIGHMEM_START + 4096)

#define ALLOC_LOWMEM_SHIFT	1
#define ALLOC_NORMAL_SHIFT	2
#define ALLOC_ZEROED_SHIFT	3

#define ALLOC_LOWMEM	(1<<ALLOC_LOWMEM_SHIFT)
#define ALLOC_NORMAL	(1<<ALLOC_NORMAL_SHIFT)
#define ALLOC_ZEROED	(1<<ALLOC_ZEROED_SHIFT)




extern void
mem_init(multiboot_info_t *mbi);

extern u32
get_free_page(void);

extern void
free_page(u32 addr);

extern u32
alloc_pages(s32 flag, s32 order);

extern void
free_pages(s32 order, u32 addr);

u32 get_zero_page(void);


//void list_del_page(unsigned long addr);
//void set_page_order(struct page *page, int order);
extern s32 get_order(u32 size);
//void buddy_show(int order);
#endif /* SHAUN_MM_H_ */


