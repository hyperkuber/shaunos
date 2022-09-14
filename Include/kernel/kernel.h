/*
 * kernel.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_KERNEL_H_
#define SHAUN_KERNEL_H_

#include <kernel/errno.h>
#include <asm/system.h>

#define packed	__attribute__((__packed__))


#define KERNEL_CS 	(1<<3)
#define KERNEL_DS 	(2<<3)

#define USER_CS		(4<<3)
#define USER_DS		(5<<3)

#define REALMODE_CS		(6<<3)
#define REALMODE_DS		(7<<3)

#define __PAGE_OFFSET	0xC0000000


#define wmb() \
	do {__asm__ __volatile__("nop;":::"memory");} while(0)

#define sti()	\
	do {__asm__ __volatile__("sti;":::);} while (0)

#define cli()	\
	do {__asm__ __volatile__("cli;":::);} while (0)

extern int printk( const char *format, ...);
#define panic(s)	\
	do { printk("Kernel Panic:%s\n", s); while(1); } while(0)


/*
 * insert an element right after the head every time got called.
 * that is to say, the last element inserted is at
 * the queue head,right after _head_ entry
 * NOTE: these two functions only can be used
 * for the structs that declared as blow:
 * struct A {
 * 		struct A *next;
 * 		struct A *prev;
 * 		.
 * 		.
 * 		.
 * };
 * struct A head, a, b, c, d, e;
 * insque(&a, &A);
 * insque(&b, &A);
 * insque(&c, &A);
 * insque(&d, &A);
 * insque(&e, &A);
 * the queue may like this:
 * head A->e->d->c->b->a
 * NOTE2:the next and prev pointer's order __must__
 * be as the same as above stated, but which name is without concerned.
 *
 * mainly used for network structs:
 * struct llinfo_arp ;
 * struct inpcb;
 * struct rawcb;
 */

static void inline insque(void *entry, void *head)
{
		asm volatile (
				"movl	(%0), %%ecx\n\t"	//ecx = head->next
				"movl	%1,	4(%%ecx)\n\t"	//head->next->prev = entry
				"movl	%%ecx,	(%1)\n\t"	//entry->next = head->next
				"movl	%1,	(%0)\n\t"		//head->next = entry
				"movl	%0,	4(%1)\n\t"		//entry->prev = head
				:/*no output */
				:"a"(head), "d"(entry)
				:"%ecx", "memory");
}
static void inline remque(void *entry)
{
	asm volatile (
			"movl	4(%0),	%%ecx\n\t"	//ecx = entry->prev
			"movl	(%0),	%%edx\n\t"	//edx = entry->next
			"movl	%%edx,	(%%ecx)\n\t"	//entry->prev->next = entry->next
			"movl	%%ecx,	4(%%edx)\n\t"	//entry->next->prev = entry->prev
			"movl	$0,		(%%eax)\n\t"	//entry->next = NULL
			"movl	$0,		4(%%eax)\n\t"	//entry->prev = NULL
			:/*no output */
			:"a"(entry)
			:"%edx", "%ecx", "memory");
}

/*
 * Disable and restore the hardware interrupts,
 * saving the interrupt enable flag into the supplied variable.
 */
static void inline __attribute__ ((always_inline))
local_irq_save (int *x)
{
	asm volatile (
	"pushf \n"
	"pop %0 \n"
	"cli"
	: "=r" (*(x)) : : "memory");
}

/*
 * Restore the hardware interrupt mode using the saved interrupt state.
 */
static void inline __attribute__ ((always_inline))
local_irq_restore (int x)
{
	asm volatile (
	"push %0 \n"
	"popf"
	: : "r" (x) : "memory");
}




#define min(a, b)	\
	((a) > (b) ? (b) : (a))

#define max(a, b) 	\
	((a) > (b) ? (a) : (b))


#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))


enum {
	KLOG_LEVEL_DEBUG = 0,
	KLOG_LEVEL_INFO,
	KLOG_LEVEL_WARN,
	KLOG_LEVEL_NOTE,
	KLOG_LEVEL_ERROR,
	KLOG_LEVEL_NONE
};

extern u32 curr_log_level;
extern u32	gfxisr_mask;


#define GFXISR_MOUSE	(1<<1)
#define GFXISR_KBD		(1<<2)
#define GFXISR_ACT		(1<<31)

#define setsoftgfx()	(pt_wakeup(&(gfx_isr_thread->kthread)))

#define schedgfxisr(x)	\
		do { gfxisr_mask |= (x);	\
		if ((gfxisr_mask & GFXISR_ACT) == 0)	\
			setsoftgfx();	\
		} while (0)




#define DEBG

#ifdef DEBG
#define LOG_ERROR(format, ...)	\
do {	\
	if (curr_log_level <= KLOG_LEVEL_ERROR)	\
		printk("\033[31mKERNEL ERROR\033[0m %s line:%d,"format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__);	\
 } while (0)

#define LOG_INFO(format, ...)	\
do {	\
	if (curr_log_level <= KLOG_LEVEL_INFO)	\
		printk("KERNEL INFO %s,"format"\n", __FUNCTION__, ##__VA_ARGS__);	\
} while (0)

#define LOG_DEBG(format, ...)	\
do {	\
	if (curr_log_level <= KLOG_LEVEL_DEBUG)	\
		printk("KERNEL DEBG %s,"format"\n", __FUNCTION__, ##__VA_ARGS__);	\
} while (0)

#define LOG_WARN(format, ...)	\
do {	\
	if (curr_log_level <= KLOG_LEVEL_WARN)	\
		printk("KERNEL WARN %s,"format"\n", __FUNCTION__, ##__VA_ARGS__);	\
} while (0)

#define LOG_NOTE(format, ...)	\
do {	\
	if (curr_log_level <= KLOG_LEVEL_NOTE)	\
		printk("KERNEL NOTE %s,"format"\n", __FUNCTION__, ##__VA_ARGS__);	\
} while (0)


#else
#define LOG_ERROR(format, ...)
#define LOG_INFO(format, ...)
#endif

#define BUG()	\
	do {int x; local_irq_save(&x); \
	LOG_ERROR("kernel bug");	\
	while (1);} while (0)

struct kernel_object {
	char *name;
	int (*init)(void);
	int (*destroy)(void *arg);
	unsigned long flag;
	struct kernel_object *next;
};


#endif /* SHAUN_KERNEL_H_ */
