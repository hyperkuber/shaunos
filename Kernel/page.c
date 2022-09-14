/*
 * page.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <kernel/page.h>
#include <kernel/mm.h>
#include <string.h>
#include <kernel/cpu.h>
#include <kernel/assert.h>
#include <kernel/kthread.h>

extern u32 nr_page_tables;
extern u32 total_mem_size;
u32 global_pg_dir[1024] __attribute__ ((__section__ (".bss.page_aligned")));

extern u32 pg0[];

u32 PAGING_PAGES;
u32 kern_pgd_max;


s32 map_page(u32 va, u32 pa, u32 cr3, s32 flags, kprocess_t *proc)
{
	s32 pgd_idx = PDE_INDEX(va);
	s32 pgt_idx = PTE_INDEX(va);
	u32 *pgd = (u32 *)VA(cr3);
	u32 pde;

	if (flags & PAGE_PSE){
		*(pgd + pgd_idx) = (pa | flags);
		return 0;
	}


	if ((*(pgd + pgd_idx) & PAGE_PRE) == 0){
		pde = get_zero_page();
		*(pgd+pgd_idx) = (PA(pde) | PAGE_RW| flags); //set pde, page table always writable
		if (proc != NULL){
			vm_insert(proc, pde, PAGE_SIZE, VM_PGT);
		}

	} else {
		pde = VA(*(pgd+pgd_idx));
		pde &= 0xFFFFF000;
//		*(pgd+pgd_idx) = (PA(pde) | flags | PAGE_RW);

	}
	//pa must be page aligned
	assert((pa & 0xFFF) == 0);

	if (*((u32 *)pde + pgt_idx) & 0xFFFFF000) {
		LOG_ERROR("va :%x  already mapped.(conflict.)", va);
		BUG();
		return -1;
	}

	*((u32 *)pde + pgt_idx) = (pa | flags);
	u32 cr = 0;
	asm volatile (
			"movl %%cr3, %0\n\t"
			"movl %0, %%cr3\n\t"
			:
			:"r"(cr));
	return 0;

}



s32 map_pages(u32 va, u32 size, s32 flag, kprocess_t *proc)
{
	u32 dest = va & (~(PAGE_SIZE -1));
	u32 cr3;
	u32 page_offset = va & 0xFFF;
	if (proc == NULL){
		cr3 = get_cr3();
	} else
		cr3 = proc->cr3;

	LOG_INFO("va:%x dest:%x", va, dest);

	flag |= PAGE_PRE;
	s32 i = PAGE_ALIGN(size) >> PAGE_SHIFT;
	if ((page_offset + size % PAGE_SIZE) > PAGE_SIZE)
		i++;

	if (va < __PAGE_OFFSET)
		flag |= PAGE_USR;
	LOG_INFO("nr_pg:%d", i);
	for (;i != 0; i--){
		u32 page = get_zero_page();
		assert(page != 0);
		if (proc != NULL){
			vm_insert(proc, page, PAGE_SIZE, VM_COMMON);
		}
		//LOG_NOTE("mapping page:%x, cr3:%x", dest, cr3);
		if (map_page(dest, PA(page), cr3, flag, proc) < 0)
			return -1;	//BUGFIX:rollback, free pages allocated.
		dest += PAGE_SIZE;

	}

	return 0;
}

void paging_init(multiboot_info_t *mbi )
{
	s32 pgd = __PAGE_OFFSET >> 22;
	u32 phyaddr = 0x00000000;
	u32 *p =(u32 *)((u32)global_pg_dir - __PAGE_OFFSET);
	total_mem_size = mbi->mem_lower + mbi->mem_upper;	// in kilobytes
	// assume that your cpu support pse
	if (!cpu_has_pse()) {
		LOG_ERROR("cpu does not support pse, kernel start failed");
		while(1);
	}
	//this is a beautiful bug figured out by vmware
	asm volatile (
			"movl	%%cr4, %%eax\n\t"
			"orl	$0x10,	%%eax\n\t"
			"movl	%%eax,	%%cr4\n\t"
			:::"memory");

	for (; pgd < 1024 && phyaddr < (total_mem_size * 1024); ){
		*(p+pgd) = (phyaddr | PAGE_PRE | PAGE_RW | PAGE_PSE | PAGE_GLOBAL);
		pgd++;
		phyaddr += 0x400000;

	}
	//identify map first 4M, and pg0 is free to use
	*p = *(p+ (__PAGE_OFFSET >> 22));


	__asm__ __volatile__ (
				"movl	%%cr4,	%%eax\n\t"
				"orl	$0x80,	%%eax\n\t"			//enable global page
				"movl	%%eax,	%%cr4\n\t"
				"movl	%0,	%%cr3\n\t"				//flush tlb
				"ljmp	$0x08,	$1f\n\t"			//flush prefetch and eip
				"1:"
				::"r"((u32)global_pg_dir - __PAGE_OFFSET):"memory","%eax");
	//somewhat like max_low_pfn in linux kernel
	kern_pgd_max = pgd;
}
