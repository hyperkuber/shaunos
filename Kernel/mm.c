/*
 * mm.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <asm/system.h>
#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <kernel/malloc.h>
#include <kernel/page.h>
#include <string.h>
#include <list.h>
#include <kernel/bitops.h>
//#include <kernel/assert.h>


#define BUDDY_CHUNK_NUM	11
#define MAX_BUDDY_BITS	16	//for 1G memory

//convert a page address to kernel virtual address




struct page {
	u32 flag;
	u32 count;
	u32 private;
	struct linked_list list;
};

struct page *mem_map;

u32 total_mem_size;
u32 total_pages;

struct page *mem_lower_map;
struct page *mem_upper_map;
struct page *kern_mem_end;
struct page *buddy_map_base;
u32 paging_pages;

extern u32 pg0[];



struct mm_buddy {
	u32 order;
	u32 free_num;
	struct linked_list list;
};

struct mm_buddy mm_buddy_array[BUDDY_CHUNK_NUM];

static inline u32
page_to_virt(struct page *p)
{
	return (((p - mem_upper_map) << PAGE_SHIFT) + 0xC0100000UL);
}

static inline struct page *
virt_to_page(u32 addr)
{
	return (struct page *)((((addr) - 0xC0100000UL) >> PAGE_SHIFT) + mem_upper_map);
}

s32 buddy_init(u32 total_mem,struct page *start, struct page *end)
{
	s32 i;
	struct page *p = start;
	for (i=0; i<BUDDY_CHUNK_NUM; i++){
		mm_buddy_array[i].order = i;
		mm_buddy_array[i].free_num = 0;
		list_init(&mm_buddy_array[i].list);
	}

	i--;
	//list_init(&start->list);
	for (; (u32)p <= (u32)end; p++){
		//list_init(&p->list);
		//list_add_tail(&mm_buddy_array[i].list, &(p->list));
		list_init(&p->list);
		p->count = 0;
		p->private = i;
		set_bit(PAGE_BUDDY, &p->flag);
	}
	//list_init(&start->list);
	list_add(&mm_buddy_array[i].list, &start->list);
	//start->private = i;
	buddy_map_base = p;

	mm_buddy_array[i].free_num = (total_mem >> 2) >> i;

	LOG_DEBG("buddy array %d free num:%x", i, mm_buddy_array[i].free_num);

	return 0;
}
static inline void
rmv_page_order(struct page *page)
{
	clear_bit(PAGE_BUDDY, &page->flag);
	page->private = 0;
}
static inline void
set_page_order(struct page *page, s32 order)
{
	page->private = order;
	set_bit(PAGE_BUDDY, &page->flag);
}

struct page *expand(struct page *p, s32 order, s32 curr_order)
{
//	static inline struct page *
//	expand(struct zone *zone, struct page *page,
//	 	s32 low, s32 high, struct free_area *area)
//	{
//		u32 size = 1 << high;
//
//		while (high > low) {
//			area--;
//			high--;
//			size >>= 1;
//			BUG_ON(bad_range(zone, &page[size]));
//			list_add(&page[size].lru, &area->free_list);
//			area->nr_free++;
//			set_page_order(&page[size], high);
//		}
//		return page;
//	}

	u32 size = 1 << curr_order;

	if (curr_order == BUDDY_CHUNK_NUM -1){
		list_add(&mm_buddy_array[curr_order].list, &p[size].list);
	}

	while (curr_order > order) {
		curr_order--;
		size >>= 1;
		list_add(&mm_buddy_array[curr_order].list, &p[size].list);
		mm_buddy_array[curr_order].free_num++;
		set_page_order(&p[size], curr_order);
	}
	return p;
}

struct page *buddy_alloc(s32 order)
{
//	struct free_area * area;
//	unsigned s32 current_order;
//	struct page *page;
//
//	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
//		area = zone->free_area + current_order;
//		if (list_empty(&area->free_list))
//			continue;
//
//		page = list_entry(area->free_list.next, struct page, lru);
//		list_del(&page->lru);
//		rmv_page_order(page);
//		area->nr_free--;
//		zone->free_pages -= 1UL << order;
//		return expand(zone, page, order, current_order, area);
//	}
	struct page *p;
	s32 corder;
	for (corder=order; corder<BUDDY_CHUNK_NUM; corder++) {
		if (list_is_empty(&mm_buddy_array[corder].list))
			continue;

		p = container_of(mm_buddy_array[corder].list.next, struct page, list);
		list_del(&p->list);
		rmv_page_order(p);
		mm_buddy_array[corder].free_num--;
		return expand(p, order, corder);
	}
	return NULL;
}

/*
 * this is the buddy core algorithm,
 * i handle entire buddy list,which in linux,
 * only head page was moved,
 * what i did may take a little performance lost,
 * but easy to understand, and it works well
 *
 * by the way, i __really__ do not want to
 * debug it,
 */

//struct page *buddy_alloc(s32 order)
//{
//	struct page *p;
//	struct linked_list *pos, *head;
//	s32 new_order;
//	s32 size;
//	s32 i;
//AGAIN:
//	if (mm_buddy_array[order].free_num != 0){
//		pos = mm_buddy_array[order].list.next;
//		p = container_of(pos, struct page, list);
//		if (mm_buddy_array[order].free_num == 1)
//			list_cut(&(mm_buddy_array[order].list), &(p->list));
//		else {
//			//linux use p[size] to locate the page to cut
//			//and it is O(1), but here, O(n)
//			for (i=0; i<(1<<order); i++){
//				pos = pos->next;
//			}
//			p = container_of(pos, struct page, list);
//			pos = list_cut(&(mm_buddy_array[order].list), &(p->list));
//		}
//
//		p = container_of(pos, struct page, list);
//		mm_buddy_array[order].free_num--;
//		clear_bit(PAGE_BUDDY, &(p->flag));
//
//		for (head=pos, pos=pos->next; pos != head;
//				pos=pos->next){
//			p = container_of(pos, struct page, list);
//			clear_bit(PAGE_BUDDY, &p->flag);
//			p->private = order;
//		}
//		p = container_of(head, struct page, list);
//		p->private = order;
//
//		return p;
//	}
//
//	new_order = order;
//	while (mm_buddy_array[new_order].free_num == 0){
//		new_order++;
//		if (new_order == BUDDY_CHUNK_NUM)
//			return NULL/*-ENOMEM*/;
//	}
//
//
//	pos = mm_buddy_array[new_order].list.next;
//
//	p = container_of(pos, struct page, list);
//
//	size = (1<<new_order);
//	while (new_order > order){
//		if (mm_buddy_array[new_order].free_num == 1)
//			pos = list_cut(&(mm_buddy_array[new_order].list), &(p->list));
//		else {
//			for (i=0; i<(1<<new_order); i++){
//				pos = pos->next;
//			}
//			p = container_of(pos, struct page, list);
//			pos = list_cut(&(mm_buddy_array[new_order].list), &p->list);
//		}
//		mm_buddy_array[new_order].free_num--;
//		assert(pos != NULL);
//		size >>= 1;
//		new_order--;
//		list_splice_tail(&(mm_buddy_array[new_order].list), pos);
//		mm_buddy_array[new_order].free_num += 2;
//		//LOG_DEBG("new order %d free num:%d",new_order, mm_buddy_array[new_order].free_num);
//		pos = mm_buddy_array[new_order].list.next;
//		p = container_of(pos, struct page, list);
//		p->private = new_order;
//	}
//	goto AGAIN;
//
//	return p;
//
//}

s32 page_in_buddy(struct page *p)
{
	return get_bit(PAGE_BUDDY, &p->flag);
}

s32 page_is_buddy(struct page *p, s32 order)
{
	if (p->private == order && page_in_buddy(p) && p->count == 0)
		return 1;

	return 0;
}
s32 page_is_ok(struct page *p)
{
	if ( get_bit(PAGE_RESVD, &p->flag))
		return 0;
	return 1;
}

s32 get_order(u32 size)
{
	s32 order = 0;
	size >>= PAGE_SHIFT;
	while (size >>= 1)
		order++;
	return order;
}
void buddy_free(struct page *page, s32 order)
{
//	u32 page_idx;
//	struct page *coalesced;
//	s32 order_size = 1 << order;
//
//	if (unlikely(order))
//		destroy_compound_page(page, order);
//
//	page_idx = page - base;
//
//	BUG_ON(page_idx & (order_size - 1));
//	BUG_ON(bad_range(zone, page));
//
//	zone->free_pages += order_size;
//	while (order < MAX_ORDER-1) {
//		struct free_area *area;
//		struct page *buddy;
//		s32 buddy_idx;
//
//		buddy_idx = (page_idx ^ (1 << order));
//		buddy = base + buddy_idx;
//		if (bad_range(zone, buddy))
//			break;
//		if (!page_is_buddy(buddy, order))
//			break;
//		/* Move the buddy up one level. */
//		list_del(&buddy->lru);
//		area = zone->free_area + order;
//		area->nr_free--;
//		rmv_page_order(buddy);
//		page_idx &= buddy_idx;
//		order++;
//	}
//	coalesced = base + page_idx;
//	set_page_order(coalesced, order);
//	list_add(&coalesced->lru, &zone->free_area[order].free_list);
//	zone->free_area[order].nr_free++;

	struct page *coalesced;
	//s32 order_size = 1<<order;
	u32 page_idx;
	page_idx = page-buddy_map_base;

	while (order < BUDDY_CHUNK_NUM -1) {
		struct page *buddy;
		s32 buddy_idx;
		buddy_idx = (page_idx ^ (1 << order));
		buddy = buddy_map_base + buddy_idx;
		if (!page_is_buddy(buddy, order))
			break;
		if (!page_is_ok(buddy))
			break;
		list_del(&buddy->list);
		mm_buddy_array[order].free_num--;
		rmv_page_order(buddy);
		page_idx &= buddy_idx;
		order++;
	}
	coalesced = buddy_map_base + page_idx;
	set_page_order(coalesced, order);
	list_add(&mm_buddy_array[order].list, &coalesced->list);
	mm_buddy_array[order].free_num++;
}

//void buddy_free(struct page *p, s32 order)
//{
//	s32 i;
//	s32 new_order = order;
//	struct linked_list *pos;
//	struct page *buddy;
//	u32 pge_idx = p - buddy_map_base;
//	u32 buddy_idx;
//	struct linked_list coalesced;
//	list_init(&coalesced);
//
//
//	list_splice_tail(&coalesced, &(p->list));
//	while (new_order < BUDDY_CHUNK_NUM - 1){
//		buddy_idx = pge_idx ^ (1<<new_order);
//		buddy = buddy_map_base + buddy_idx;
//
//		if (!page_in_buddy(buddy))
//			break;
//
//		if (!page_is_buddy(buddy, new_order))
//			break;
//
//
//		//LOG_DEBG("pge_idx:%x page:%x buddy_idx:%x buddy:%x order:%d", pge_idx, page_to_virt(p),
//		//		buddy_idx, page_to_virt(buddy), new_order);
//
//		if (mm_buddy_array[new_order].free_num == 1){
//			pos = list_cut((buddy->list.prev), &(buddy->list));
//		}
//		else if (mm_buddy_array[new_order].free_num > 1) {
//			pos = &buddy->list;
//			for (i=0; i<(1<<new_order); i++)
//				pos = pos->next;
//			struct page *p1 = container_of(pos, struct page, list);
//			pos = list_cut((buddy->list.prev), &(p1->list));
//		}
//		else
//			break;
//
//
//		mm_buddy_array[new_order].free_num--;
//
//
//		if ((u32)p < (u32)buddy)
//			list_splice_tail(&coalesced, pos);
//		else
//			list_splice(&coalesced, pos);
//
//		pge_idx &= buddy_idx;
//		new_order++;
//
//	}
//
//	for (pos = coalesced.next; pos != &coalesced; pos = pos->next){
//		p = container_of(pos, struct page, list);
//		p->private = new_order;
//		set_bit(PAGE_BUDDY, &p->flag);
//	}
//	pos = coalesced.next;
//	list_del(&coalesced);
//	list_splice_tail(&(mm_buddy_array[new_order].list), pos);
//	mm_buddy_array[new_order].free_num++;
//
//}

/*
 * for debug buddy system
 */
void buddy_show(s32 order)
{
	struct linked_list *pos;
	struct page *p;
	s32 i;
	LOG_INFO("mm_budd_array[%d].free_num:%d", order, mm_buddy_array[order].free_num);
	for (i = 0,pos = mm_buddy_array[order].list.next; pos != &mm_buddy_array[order].list;
			pos = pos->next, i++){
		p = container_of(pos, struct page, list);
		LOG_INFO("idx:%d page:%x ",i, (u32)0xC0100000 + ((p - mem_upper_map) * 4096));
	}

}



u32 get_free_page(void)
{
	return alloc_pages(ALLOC_NORMAL, 0);
}

u32 get_zero_page(void)
{
	return alloc_pages(ALLOC_NORMAL | ALLOC_ZEROED, 0);
}




u32 alloc_pages_lowmem(s32 nr)
{
	s32 i, j;
	s32 num;
	u32 retval = -1;
	struct page *p;
	u32 lowmem_pages =(u32)(mem_upper_map - mem_lower_map);
	if (nr <= 0)
		return -1;

	for (i=0; i<lowmem_pages;){

		if (mem_lower_map[i].count == -1){
			for (j=i, num=0; num < nr; j++, num++){
				if (mem_lower_map[j].count != -1)
					goto AGAIN;
			}

			p = &mem_lower_map[i];
			if (!page_is_ok(p))
				goto AGAIN;

			retval = (u32)0xC0000000 + ((p - mem_lower_map) << PAGE_SHIFT);
			for (j=i, num=nr; num > 0; num--){
				mem_lower_map[j++].count++;
			}
			break;
AGAIN:
			i+=num;
			continue;
		}
		i++;
	}

	return retval;
}

u32 alloc_pages_buddy(s32 order)
{
	struct page *p = NULL;
	s32 x;
	s32 i;
	local_irq_save(&x);
	p = buddy_alloc(order);
	if (p == NULL) {
		local_irq_restore(x);
		return 0;
	}
	//set_bit(PAGE_USED, &(p->flag));
	for (i=0; i<(1<<order); i++){
		p[i].count++;
	}


	local_irq_restore(x);
	return page_to_virt(p);

}

s32 mem_calc_order(u32 size)
{
	u32 s = PAGE_ALIGN(size);
	u32 nr_pgs = s >> PAGE_SHIFT;
	s32 order = 0;
	while (nr_pgs >>= 1)
		order++;

	return order;
}

u32 __alloc_pages(s32 flag, s32 order)
{
	u32 retval;

	if (flag & ALLOC_LOWMEM){
		retval = alloc_pages_lowmem(1<<order);
		if (retval == 0)
			return retval;

		goto END;
	}

	if (flag & ALLOC_NORMAL){
		retval = alloc_pages_buddy(order);
		if (retval == 0)
			return retval;
	}

END:
	if (flag & ALLOC_ZEROED){
		memset((void *)retval, 0, (1<<order) << PAGE_SHIFT);
	}

	return retval;

}
u32 alloc_pages(s32 flag, s32 order)
{
	if (flag <= 0 || order < 0 || order > 11)
		return 0;

	return __alloc_pages(flag, order);
}

void free_page_lowmem(u32 addr)
{
	if (addr < (u32)kern_mem_end && addr > 0xC0100000)
	{
		LOG_ERROR("free_page:trying to free kernel page:%d\n", addr);
		return;
	}

	if (addr & 0xFFF){
		LOG_ERROR("free_page: invalid alignment of page addr:%x\n", addr);
		return;
	}

	addr -= (u32)0xC0000000;
	addr >>= PAGE_SHIFT;
	if (mem_lower_map[addr].count == -1)
		return;
	mem_lower_map[addr].count--;

}

void free_pages_lowmem(s32 nr, u32 addr)
{
	s32 i;
	for(i=0; i<nr; i++){
		free_page_lowmem(addr);
		addr += PAGE_SIZE;
	}

}

void free_pages_buddy (s32 order, u32 addr)
{
	struct page *p;
	s32 x, i;
	if (addr & 0xFFF){
		LOG_ERROR("invalid alignment of page addr:%x\n", addr);
		return;
	}
	p = virt_to_page(addr);
	//LOG_DEBG("freeing page:%x order:%d", (u32)p, p->private);
	if (page_in_buddy(p))
		return;



	local_irq_save(&x);
	for (i=0; i<(1<<order); i++){
		if (p[i].count - 1 != 0) {
			LOG_INFO("p[i].count:%d private,%d flag:%x addr:%x.", p[i].count, p[i].private, p[i].flag,
					page_to_virt(p));

			BUG();
		}
		if ( p[i].count - 1 != 0)
			goto end;
		p[i].count--;

		//LOG_INFO("");
		//Assert(p[i].count == 0);
	}
	buddy_free(p, order);
end:
	local_irq_restore(x);
}

void free_page(u32 addr)
{
	free_pages(0, addr);
}

void free_pages(s32 order, u32 addr)
{
	if (addr < 0xC0100000)
		free_pages_lowmem(1<<order, addr);
	else
		free_pages_buddy(order, addr);
}

void set_pageaddr_order(u32 addr, s32 order)
{
	if ((addr & 0xFFF))
		return;
	struct page *p = virt_to_page(addr);

	p->private = order;

}

void list_del_page(u32 addr)
{
	if ((addr & 0xFFF))
		return;

	struct page *p = virt_to_page(addr);

	list_del(&p->list);
	p->private = 0;
	//buddy_free(p, 0);

}


void mem_init(multiboot_info_t *mbi)
{
	u32 phyaddr = 0x1000;
	s32 i;
	u32 mem_lower = mbi->mem_lower;
	u32 mem_upper;
	struct page *start, *end;
	if (mbi->mem_upper * 1024 < 4 *1024 *1024)
		panic("memory size must be larger than 4M");
	// only support 1G memory
	if (mbi->mem_upper * 1024 > 0x40000000)
		mem_upper = 0x40000000 / 1024;	// in kb
	else
		mem_upper = mbi->mem_upper;

	LOG_INFO("mem_upper:%d mem lower:%d total memsize:%dM",mem_upper, mem_lower, total_mem_size/1024);

	total_pages = (((total_mem_size * 1024) & PAGE_MASK) >> PAGE_SHIFT);

	mem_lower_map = (struct page *)((u32)pg0 + 4096 + 0x200000);
	LOG_INFO("mem_lower_map start:%x", (u32)mem_lower_map);
	for (i=0; phyaddr < mem_lower * 1024; phyaddr += 0x1000, i++){
		mem_lower_map[i].count = -1;
		mem_lower_map[i].flag = 0;
		list_init(&(mem_lower_map[i].list));
	}
	//mark first two pages as reserved,
	//for where bios data was located
	set_bit(PAGE_RESVD, &mem_lower_map[0].flag);
	set_bit(PAGE_RESVD, &mem_lower_map[1].flag);
	LOG_INFO("mem_lower_map nums: %d", i);
	mem_upper_map = mem_lower_map + i;

	LOG_INFO("mem_upper_map start: %x", (u32)mem_upper_map);
	paging_pages += i;
	for (phyaddr = 0x100000, i=0; phyaddr < mem_upper * 1024; phyaddr += 0x1000, i++){
		mem_upper_map[i].count = -1;
		mem_upper_map[i].flag = 0;
		list_init(&(mem_upper_map[i].list));
	}

	start = mem_upper_map;
	end = mem_upper_map + i;

	kern_mem_end =(struct page *)(((u32)(mem_upper_map + i) + PAGE_SIZE -1) & PAGE_MASK);
	paging_pages += i;
	LOG_INFO("kern_mem_end:%x\npaing pages:%d", (u32)kern_mem_end, paging_pages);
	// mask kernel page as used
	for (phyaddr = 0xC0100000, i=0;
			phyaddr < (u32)kern_mem_end;
			phyaddr +=0x1000, i++){
		mem_upper_map[i].count = 1;
		set_bit(PAGE_RESVD, &(mem_upper_map[i].flag));
	}

	buddy_init(mem_upper, &mem_upper_map[i], end);

	paging_pages -= i;

//	#define DEBG_MM

#ifdef DEBG_MM
	u32 addr1, addr2, addr;

	LOG_INFO("get first free page phyaddr:%x",addr1 = alloc_pages(ALLOC_NORMAL, 10));
	buddy_show(0);
	LOG_INFO("get second free page phyaddr:%x", addr2 = alloc_pages(ALLOC_NORMAL, 10));
	buddy_show(0);
	LOG_INFO("get third free page phyaddr:%x", addr = alloc_pages(ALLOC_NORMAL, 0));
	buddy_show(0);

	LOG_INFO("free first page:%x", addr1);

	free_pages(10, addr1);
	buddy_show(0);
	LOG_INFO("free second page:%x", addr2);
	free_pages(10, addr2);
	buddy_show(0);

	LOG_INFO("free third page:%x", addr);
	free_page(addr);
	buddy_show(1);
	buddy_show(0);
	buddy_show(2);
	LOG_INFO("get third free page phyaddr:%x", addr = alloc_pages(ALLOC_NORMAL, 1));
	buddy_show(1);
	free_page(addr);
	buddy_show(1);

	LOG_INFO("get fourth free page phyaddr:%x",addr1 = alloc_pages(ALLOC_NORMAL, 2));
	buddy_show(2);
	free_page(addr1 + PAGE_SIZE);
	buddy_show(2);
	while (1);
//	set_page_order(addr1+PAGE_SIZE, 1);
//	list_del_page(addr1);
//	free_page(addr1);
//	list_del_page(addr1+PAGE_SIZE * 3);
//	free_page(addr1+PAGE_SIZE * 3);
//	buddy_show(0);
//	set_page_order(addr1+PAGE_SIZE, 0);
//	free_page(addr1+PAGE_SIZE);
//	buddy_show(2);
//
//	while (1);

//	for (i=0; i<3; i++)
//		buddy_show(i);

	LOG_INFO("get second free page phyaddr:%x", get_free_page());

	LOG_INFO("get the third page :%x",addr=get_free_page());
	free_page(addr);
	LOG_INFO("get the third page :%x",addr=get_free_page());
	for (i=0; i<10; i++)
		LOG_INFO("get the %d page :%x",i, get_free_page());

#endif


}
