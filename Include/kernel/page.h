/*
 * page.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_PAGE_H_
#define SHAUN_PAGE_H_

#include <kernel/kernel.h>
#include <kernel/kthread.h>
#include <kernel/mm.h>
#include <kernel/types.h>
#include <kernel/multiboot.h>


#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PAGE_ALIGN(x)	(((x) + PAGE_SIZE -1) & PAGE_MASK)

#define PG_BIT_PRESENT	0	/* is present */
#define PG_BIT_RW		1	/* writeable */
#define PG_BIT_USER		2	/* userspace addressable */
#define PG_BIT_PWT		3	/* page write through */
#define PG_BIT_PCD		4	/* page cache disabled */
#define PG_BIT_ACCESSED	5	/* was accessed (raised by CPU) */
#define PG_BIT_DIRTY		6	/* was written to (raised by CPU) */
#define PG_BIT_PSE		7	/* 4 MB (or 2MB) page */
#define PG_BIT_PAT		7	/* on 4KB pages */
#define PG_BIT_GLOBAL	8	/* Global TLB entry PPro+ */




#define PAGE_PRE	((1UL) << PG_BIT_PRESENT)
#define PAGE_RW		((1UL) << PG_BIT_RW)
#define PAGE_USR	((1UL) << PG_BIT_USER)
#define PAGE_PWT	((1UL) << PG_BIT_PWT)
#define PAGE_PCD	((1UL) << PG_BIT_PCD)
#define PAGE_ACCESSED	((1UL) << PG_BIT_ACCESSED)
#define PAGE_DIRTY	((1UL) << PG_BIT_DIRTY)
#define PAGE_PSE	((1UL) << PG_BIT_PSE)
#define PAGE_GLOBAL	((1UL) << PG_BIT_GLOBAL)
#define PAGE_PAT	((1UL) << PG_BIT_PAT)

#define PAGE_AVAIL_SHIFT	0
#define PAGE_USED_SHIFT		1
#define PAGE_LOCKED_SHIFT	2
#define PAGE_RESVD_SHIFT	3
#define PAGE_BUDDY_SHIFT	4


#define PAGE_AVAIL	(0)
#define PAGE_USED	(1<<PAGE_USED_SHIFT)
#define PAGE_LOCKED	(1<<PAGE_LOCKED_SHIFT)
#define PAGE_RESVD	(1<<PAGE_RESVD_SHIFT)
#define PAGE_BUDDY	(1<<PAGE_BUDDY_SHIFT)


#define __PAGE_OFFSET	0xC0000000
#define PA(x)	((unsigned long)(x) - __PAGE_OFFSET)
#define VA(x)	(((unsigned long)(x)) + __PAGE_OFFSET)

#define PDE_INDEX(x)	(((x) & 0xFFC00000) >> 22)
#define PTE_INDEX(x)	(((x) & 0x3FF000) >> 12)


#define MAX_ARG_PAGES	2
#define MAX_ENV_PAGES	2
#define ENV_BASE		(__PAGE_OFFSET - (MAX_ENV_PAGES << PAGE_SHIFT))
#define ARG_BASE		(ENV_BASE - (MAX_ARG_PAGES << PAGE_SHIFT))
#define USER_STACK_BASE	(ARG_BASE)
#define USER_STACK_TOP	(USER_STACK_BASE - (1<<30))
#define USER_STACK_LAZY (USER_STACK_BASE - (2 << PAGE_SHIFT))


typedef struct page_table_entry_desc {
	uint_t 	present:1 packed;
	uint_t	rw:1 packed;
	uint_t	us:1 packed;
	uint_t	pwt:1 packed;
	uint_t	pcd:1 packed;
	uint_t	accessed:1 packed;
	uint_t	dirty:1	packed;
	uint_t	pat:1	packed;
	uint_t	global:1 packed;
	uint_t	avl:3	packed;
	uint_t	baseaddr:20	packed;

} pte_t;

typedef struct page_directory_entry_desc {
    uint_t present:1 packed;
    uint_t rw:1 packed;
    uint_t us:1 packed;
    uint_t pwt:1 packed;
    uint_t pcd:1 packed;
    uint_t accesed:1 packed;
    uint_t reserved:1 packed;
    uint_t ps:1 packed;
    uint_t global:1 packed;
    uint_t kinfo:3 packed;
    uint_t baseaddr:20 packed;
} pde_t ;

struct kernel_process;

void paging_init(multiboot_info_t *mbi);
int map_page(unsigned long va, unsigned long pa, unsigned long cr3, int flags, struct kernel_process *proc);
int map_pages(unsigned long va, unsigned long size, int flag, struct kernel_process *proc);
#endif /* SHAUN_PAGE_H_ */
