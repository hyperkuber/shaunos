/*
 * mouse.c
 *
 *  Created on: Mar 28, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <asm/io.h>
#include <kernel/mouse.h>
#include <kernel/io_dev.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>
#include <kernel/kthread.h>
#include <kernel/gfx/gfx.h>

/*
 * Mouse output bits.
 *
 *     	MOUSE_START_FRAME	Start of report frame bit.
 *	MOUSE_X_SIGN		Sign bit for X.
 *	MOUSE_Y_SIGN		Sign bit for Y.
 *	MOUSE_X_OFFSET		X offset to start cursor at.
 *	MOUSE_Y_OFFSET		Y offset to start cursor at.
 */


/*
 * Definitions for mouse buttons
 */


#define MOUSE_INPUT_QSIZE	512
#define MOUSE_INPUT_QMASK	0x3F
#define NEXT(index)	(((index)+1) & MOUSE_INPUT_QMASK)



static int mouse_cycle = 0;
char mouse_b[4];

static int inline
ms_putq(struct moused *pms, struct mouse_input *mi)
{
	if (NEXT(pms->ms_inqtail) != pms->ms_inqhead) {
		pms->ms_inbuf[pms->ms_inqtail] = *mi;
		pms->ms_inqtail = NEXT(pms->ms_inqtail);
		return 0;
	}
	return -1;
}

static int inline
ms_dequeue(struct moused *pms, struct mouse_input *mi)
{
	if (pms->ms_inqhead != pms->ms_inqtail) {
		*mi = pms->ms_inbuf[pms->ms_inqhead];
		pms->ms_inqhead = NEXT(pms->ms_inqhead);
		return 0;
	}
	return -1;
}


void ms_irq_handler(int irq, void *arg, trap_frame_t *regs)
{
	struct moused *pms = (struct moused *)arg;
	struct mouse_input mi;

	u8 data;
	data = inb(0x60);
	switch (mouse_cycle++){
	case 0:
		mouse_b[0] = data;
		if (mouse_b[0] == 0x80 || mouse_b[0] == 0x40 || !(mouse_b[0] & 0x08)) {
			mouse_cycle = 0;
			return;
		}
		break;
	case 1:
		mouse_b[1] = data;
		break;
	case 2:
		mouse_b[2] = data;
		break;
	case 3:	//for wheel scroll
		mouse_b[3] = data;
		break;
	}

	if (pms->ms_flags & MOUSE_ID0) {
		if (mouse_cycle == 3) {
			mouse_cycle = 0;
			memset((void *)&mi, 0, sizeof(mi));
			mi.ms_dy = mouse_b[2];
			mi.ms_dx = mouse_b[1];
			mi.ms_state = mouse_b[0] & 0x07;

			if (mouse_b[0] & 0x20) {
				mi.ms_state |= MOUSE_Y_SIGN;
				mi.ms_dy |= 0xFFFFFF00;
			}

			if (mouse_b[0] & 0x10) {
				mi.ms_state |= MOUSE_X_SIGN;
				mi.ms_dx |= 0xFFFFFF00;
			}


			ms_putq(pms, &mi);

			//LOG_INFO("x %d,y %d, mb:%d", mi->mx, mi->my, mi->mb);
			if (pms->ms_wq){
				wakeup_waiters(pms->ms_wq);
				pms->ms_wq = NULL;
			}
		}
	} else if (pms->ms_flags & MOUSE_ID3) {
		if (mouse_cycle == 4) {
			mouse_cycle = 0;
			memset((void *)&mi, 0, sizeof(mi));
			mi.ms_dy = mouse_b[2];
			mi.ms_dx = mouse_b[1];

			//check x, y overflow
			if (mouse_b[0] & 0x20) {
				mi.ms_state |= MOUSE_Y_SIGN;
				mi.ms_dy |= 0xFF00;
			}

			if (mouse_b[0] & 0x10) {
				mi.ms_state |= MOUSE_X_SIGN;
				mi.ms_dx |= 0xFF00;
			}
			//get button state
			mi.ms_state |= (mouse_b[0] & 0x07);
			mi.ms_wheel = (mouse_b[3] & 0x0F);
			//mouse controller just send 0x08 0x00 0x00 0x00 sequence when any button was released
			if ( *((int *)mouse_b) == 0x08)
				mi.ms_state |= MOUSE_BUTTON_UP;
			ms_putq(pms, &mi);
			//LOG_INFO("x %d,y %d, mb:%d, mw:%d", mi.ms_dx, mi.ms_dy, mi.ms_state, mi.ms_wheel);
//			frame_mouse_event(&mi);
			schedgfxisr(GFXISR_MOUSE);
//			if (pms->ms_wq){
//				wakeup_waiters(pms->ms_wq);
//				pms->ms_wq = NULL;
//			}

		}
	}
}

inline void
mouse_wait(unsigned char type)
{
	unsigned time_out = 100000;
	switch (type) {
	case 0:
		while (time_out--){
			if ((inb(0x64) & 1) == 1)
				return;
		}
		return;
	case 1:
		while (time_out--){
			if ((inb(0x64) & 2) == 0)
				return;
		}
		return;
	default:
		return;
	}
}

inline void
mouse_write(unsigned char val)
{
	mouse_wait(1);
	outb(0x64, 0xD4);
	mouse_wait(1);
	outb(0x60, val);
}

unsigned char
mouse_read()
{
	mouse_wait(0);
	return inb(0x60);
}


int ms_add_waiter(dev_t *dev, wait_queue_head_t *wq)
{
	struct moused *pms = (struct moused *)dev->driver_data;
	if (pms->ms_wq == NULL) {
		pms->ms_wq = wq;
	}
	return 0;
}

int ms_read(dev_t *dev, void *buf, size_t size)
{
	struct moused *pms = (struct moused *)dev->driver_data;
	struct mouse_input mi;
	int ret;

	if (dev->inuse < 1)
		return -EPERM;

	if (!buf || size < sizeof(struct mouse_input))
		return -EINVAL;

//	/*
//	 *
//	 */
//	if (!(pms->ms_flags & MOUSE_EVENT_ON)) {
//		io_wait(&pms->ms_wq);
//	}

	memset((void *)&mi, 0, sizeof(mi));

	ret = ms_dequeue(pms, &mi);
	if (ret < 0)
		return -EAGAIN;

//	if(ret < 0) {
//		io_wait(&pms->ms_wq);
//	}
	memcpy((void *)buf, (void *)&mi, sizeof(struct mouse_input));

	pms->ms_flags &= ~MOUSE_EVENT_ON;

	return 1;
}

int ms_open(dev_t *dev)
{
	struct moused *pms = (struct moused *)dev->driver_data;

	if (pms->ms_flags & MOUSE_ENABLED)
		pms->ms_refcnt++;
	else
		return -ENODEV;

	return 0;
}

int ms_write(dev_t *dev, void *buf, size_t size)
{
	return -ENOSYS;
}

int ms_close(dev_t *dev)
{
	struct moused *pms = (struct moused *)dev->driver_data;
	//if (--pms->ms_refcnt <= 0)
	--pms->ms_refcnt;
	return 0;
}

static struct dev_ops ms_ops = {
	.open = ms_open,
	.read = ms_read,
	.write = ms_write,
	.close = ms_close,
	.io_request = NULL,
};
/*
 * to enable scroll wheel,
 * chagne mouseid from 0 to 3, at
 * the same time, the number of bytes in
 * the mouse packets changes to 4
 */
int mouse_changeid3()
{
	u8 status;
	mouse_write(0xF3);
	status = mouse_read();
	if (status != 0xFA)
		return -1;

	mouse_write(200);
	status = mouse_read();
	if (status != 0xFA)
		return -1;

	mouse_write(0xF3);
	status = mouse_read();
	if (status != 0xFA)
		return -1;
	mouse_write(100);
	status = mouse_read();
	if (status != 0xFA)
		return -1;

	mouse_write(0xF3);
	status = mouse_read();
	if (status != 0xFA)
		return -1;
	mouse_write(80);
	status = mouse_read();
	if (status !=  0xFA)
		return -1;

	mouse_write(0xF2);
	status = mouse_read();
	if (status != 0xFA)
		return -1;
	status = mouse_read();
	if (status != 3)
		return -1;
	LOG_INFO("set mosueid to %d", status);
	return 0;
}


void mouse_probe(struct moused *pms)
{
	u8 status;
	mouse_wait(1);
	outb(0x64, 0xA8);

	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	status = (inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, status);

	mouse_write(0xF6);
	status = mouse_read();
	if (status != 0xFA)	//FACK
		LOG_ERROR("mouse responded invalid status for command 0xF6");

	mouse_write(0xF4);
	status = mouse_read();
	if (status != 0xFA)
		LOG_ERROR("mouse responded invalid status for command 0xF4");

	mouse_write(0xF2);//get mouseID
	status = mouse_read();
	if (status == 0xFA) {
		status = mouse_read();
		LOG_INFO("mouseID:%d", status);
	}

	if (status == 0) {
		if (mouse_changeid3() < 0) {
			LOG_ERROR("set mouseid to 3 failed");
			pms->ms_flags |= MOUSE_ID0;
		}
		else
			pms->ms_flags |= MOUSE_ID3;
	}

}


void mouse_init()
{
	struct moused *pms;
	pms = (struct moused *)kzalloc(sizeof(struct moused));
	assert(pms != NULL);

	mouse_probe(pms);

	pms->ms_inbuf = (struct mouse_input *)kzalloc(MOUSE_INPUT_QSIZE);
	assert(pms->ms_inbuf != NULL);


	//list_init(&pms->ms_wq.waiter);
	pms->ms_refcnt++;
	pms->ms_flags |= MOUSE_ENABLED;

	register_device("PS/2 mouse", &ms_ops, MKDEVID(2,0), (void *)pms);
	register_irq(12, (u32)ms_irq_handler, (void *)pms, IRQ_REPLACE);
	enable_irq(12);
}
