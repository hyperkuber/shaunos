/*
 * mouse.h
 *
 *  Created on: Mar 30, 2013
 *      Author: root
 */

#ifndef MOUSE_H_
#define MOUSE_H_


#include <kernel/types.h>
#include <kernel/io_dev.h>

#define MOUSE_ENABLED	(1<<0)
#define MOUSE_EVENT_ON	(1<<1)
#define MOUSE_ID3		(1<<2)
#define MOUSE_ID4		(1<<3)
#define MOUSE_ID0		(1<<4)

#define MOUSE_BUTTON_UP	(1<<8)
#define EVENT_LEFT_BUTTON	0x01
#define EVENT_MIDDLE_BUTTON	0x02
#define EVENT_RIGHT_BUTTON	0x03


#define MOUSE_START_FRAME	0x80
#define MOUSE_X_SIGN		0x10
#define MOUSE_Y_SIGN		0x08

#define MS_RIGHT_BUTTON			0x02
#define MS_MIDDLE_BUTTON		0x04
#define MS_LEFT_BUTTON			0x01

struct mouse_input {
	u16 ms_state;
	s16	ms_dx;
	s16 ms_dy;
	u16 ms_wheel;
} __attribute__((__packed__));

struct moused {
	u32 ms_refcnt;
	u32 ms_flags;
	wait_queue_head_t *ms_wq;
	struct mouse_input *ms_inbuf;
	u32	ms_inqhead, ms_inqtail;
};

void mouse_init();
extern int ms_add_waiter(dev_t *dev, wait_queue_head_t *wq);
#endif /* MOUSE_H_ */
