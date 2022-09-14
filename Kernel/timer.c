/*
 * timer.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <asm/io.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/kthread.h>
#include <kernel/time.h>
#include <kernel/irq.h>
#include <kernel/idt.h>
#include <kernel/pid.h>
#include <kernel/malloc.h>

extern unsigned long need_reschedule;
u64 ticks = 0;
u64 wall_jiffies = 0;

kthread_u_t *timer_isr_thread;
static s32 timer_depth = 0;
static s32 isr_depth = 0;


static struct kid_object timer_id_obj;
static struct linked_list timer_list_head;

void timer_handler(int irq, void *data, trap_frame_t *regs);

void timer_init(void)
{
	outb(_8254CTRL_REG, _8254CTRL_VALUE);
	outb(_8254COUNTER0,	LATCH & 0xFF);
	outb(_8254COUNTER0, LATCH >> 8);

	register_irq(0, (unsigned long)timer_handler, NULL, IRQ_REPLACE);
	enable_irq(0);
	list_init(&timer_list_head);
	kernel_id_map_init(&timer_id_obj, PAGE_SIZE);

	__asm__ __volatile__("cli":::);
}

void add_timer(struct timer_list *t)
{
	list_add_tail(&timer_list_head, &t->list);
}

void del_timer(struct timer_list *t)
{
	list_del(&t->list);
}

int get_unused_timer_id()
{
	return get_unused_id(&timer_id_obj);
}

void release_timer_id(int id)
{
	put_id(&timer_id_obj, id);
}

void timer_handler(int irq, void *data, trap_frame_t *regs)
{

	unsigned nticks;
	long usec;

	ticks++;

	if (timer_depth == 0)
		timer_depth = 1;
	else
		return;


	if (current == 0)
		return;

	nticks = ticks - wall_jiffies;

	if (nticks){
		usec = nticks * INTERVAL * 1000;
		wall_jiffies += nticks;
	} else {
		usec = INTERVAL * 1000;
		wall_jiffies = ticks;
	}



	xtime.ts_nsec += usec * 1000;

	do {
		nticks--;
		if (xtime.ts_nsec > 1000000000){
			xtime.ts_nsec -= 1000000000;
			xtime.ts_sec++;
		}

	} while (nticks > 0);

	if ((regs->cs & 0x03) == RPL_USER){
		current->utime++;
	} else {
		current->ktime++; 			//in ms
	}


	schedtimerisr();

	if (current->ticks++ == 50){
		current->ticks = 0;
		need_reschedule = 1;
	}
	timer_depth = 0;

}

void *timerisr(void *args)
{
	struct linked_list *p;
	struct timer_list *t;
	while (1) {
		isr_depth = 1;
		for (p=timer_list_head.next; p != &timer_list_head;
				p = p->next){

			t = container_of(p, struct timer_list, list);

			if (t->flag & TIMER_FLAG_ENABLED) {
				if (t->flag & TIMER_FLAG_ONESHUT) {
					if (t->expires <= (u32)ticks) {
						if (t->func) {
							t->flag &= ~TIMER_FLAG_ENABLED;
							t->func((void *)t->data);
						}
					}
				}
				if (t->flag & TIMER_FLAG_PERIODIC) {
					if (t->expires <= (u32)ticks) {
						if (t->func)
							t->func((void *)t->data);

						t->expires = ticks + t->interval;
					}
				}
			}
		}
		isr_depth = 0;
		schedule();
	}
	return NULL;
}

int register_timer(int expired, int type, void (*callback)(void *), u32 arg)
{
	struct timer_list *t;
	if (expired < 0)
		return -EINVAL;
	if (((type & (TIMER_FLAG_ONESHUT | TIMER_FLAG_PERIODIC)) == 0) ||
			(type & (TIMER_FLAG_ONESHUT | TIMER_FLAG_PERIODIC)) == (TIMER_FLAG_ONESHUT | TIMER_FLAG_PERIODIC))
		return -EINVAL;

	t = (struct timer_list *)kzalloc(sizeof(*t));
	if (!t)
		return -ENOBUFS;
	list_init(&t->list);
	t->timer_id = get_unused_timer_id();

	if (callback)
		t->func = callback;
	if (type & TIMER_FLAG_ONESHUT)
		t->expires = ticks + expired;
	if (type & TIMER_FLAG_PERIODIC) {
		t->interval = expired;
		t->expires = ticks + expired;
	}


	t->flag = type;
	t->data = arg;

	add_timer(t);
	return t->timer_id;
}

int unregister_timer(int timer_id)
{
	struct linked_list *p;
	struct timer_list *t;
	if (timer_id < 0)
		return -EINVAL;

	for (p = timer_list_head.next; p != &timer_list_head; p = p->next) {
		t = container_of(p, struct timer_list, list);
		if (t->timer_id == timer_id) {
			del_timer(t);
			release_timer_id(timer_id);
			kfree(t);
			return 0;
		}
	}
	//timer not found
	return -EINVAL;
}

int timer_get_expired(int timer_id, u32 *val)
{
	struct linked_list *p;
	struct timer_list *t;
	if (timer_id < 0)
		return -EINVAL;

	for (p = timer_list_head.next; p != &timer_list_head; p = p->next) {
		t = container_of(p, struct timer_list, list);
		if (t->timer_id == timer_id) {
			*val = t->expires;
			return 0;
		}
	}
	//timer not found
	return -EINVAL;

}
int ksetitimer(int which, struct itimerval *newval, struct itimerval *oldval)
{
	return -ENOSYS;
}


void schedtimerisr()
{
	if (isr_depth == 0) {
		pt_wakeup(&timer_isr_thread->kthread);
	}

}
