/*
 * keyboard.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_KEYBOARD_H_
#define SHAUN_KEYBOARD_H_

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/io_dev.h>


#define MAX_KBSTATES            10
#define MAX_SCANCODES           0x80
#define MAX_KEYTABLES           16



#define KBSTATE_NORMAL          0
#define KBSTATE_SHIFT           1
#define KBSTATE_CTRL            2
#define KBSTATE_ALT             3
#define KBSTATE_NUMLOCK         4
#define KBSTATE_CAPSLOCK        5
#define KBSTATE_SHIFTCAPS       6
#define KBSTATE_SHIFTNUM        7
#define KBSTATE_ALTGR           8
#define KBSTATE_SHIFTCTRL       9

#define KB_DOWN	(1<<1)
#define KB_UP	(1<<2)

struct tty;
//struct key_queue {
//	int qhead, qtail;
//	u32 buf[QUEUE_SIZE];
//};

struct kbd_input {
	u16 kbd_flag;
	u16 kbd_keycode;
};

struct keytable {
	char *name;
	u16 keymap[MAX_SCANCODES][MAX_KBSTATES];
};


struct kbd {
	struct tty *k_tty;
	u32 state;
	u32 flags;
	u32 led_status;
	u32 is_E0;
	u32 ctl_keys;
	u32 kbd_inqhead;
	u32 kbd_inqtail;
	struct kbd_input *kbd_keybuf;
	struct keytable *kt;
	wait_queue_head_t *wq;
};

typedef struct key_queue keyq_t;

#define KBD_ENABLED (1<<1)
#define KBD_CANON (1<<2)
#define KBD_ENCTLALTDEL	(1<<3)
#define KBD_EVENT	(1<<4)


extern void keyboard_init(void);
extern int get_key(struct kbd *pkbd, u32 *keycode);
extern int kbd_add_waiter(dev_t *dev, wait_queue_head_t *wq);
#endif /* SHAUN_KEYBOARD_H_ */

