/*
 * timer.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_TIMER_H_
#define SHAUN_TIMER_H_

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <list.h>
#include <kernel/kthread.h>

#define CLOCK_RATE 1193180
#define INTERVAL	1	//1ms
#define SECOND	1000
#define HZ	SECOND
#define LATCH	(CLOCK_RATE * INTERVAL / SECOND)

#define INITIAL_JIFFIES ((unsigned long)(unsigned int) (-300*HZ))

#define _8254CTRL_REG	0x43	//01000011B
#define _8254COUNTER0	0x40	//01000000B

#define _8254CTRL_VALUE	0x36	//00110110B
#define PIC1_MASK               0x21    /* read mask, write mask & init data */
#define PIC2_MASK               0xa1


#define MAX_TIMER_EVENTS	5



#define TIMER_FLAG_ENABLED	(1<<0)
#define TIMER_FLAG_DISABLED	(1<<1)
#define TIMER_FLAG_ONESHUT	(1<<2)
#define TIMER_FLAG_PERIODIC	(1<<3)


struct  timer_list{
	struct linked_list list;
	u32 expires;
	u32 interval;
	s32 timer_id;
	void (*func)(void *);
	u32 data;
	u32 flag;
};

extern kthread_u_t *timer_isr_thread;
extern u64 ticks;
extern void
timer_init(void);
extern void *
timerisr(void *args);
extern int
register_timer(int expired, int type, void (*callback)(void *), u32 arg);
extern int
unregister_timer(int timer_id);
extern void
schedtimerisr();
extern int
timer_get_expired(int timer_id, u32 *val);

#endif /* SHAUN_TIMER_H_ */
