/*
 * keyboard.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <asm/io.h>
#include <kernel/types.h>
#include <kernel/keyboard.h>
#include <kernel/irq.h>
#include <kernel/idt.h>
#include <kernel/io_dev.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/assert.h>
#include <kernel/kbdus.h>
#include <kernel/kthread.h>

#define LEFTSHIFT 0x2A
#define RIGHTSHIFT 0x36

#define FLAG_BREAK     0x0080
#define FLAG_EXT       0x0100

#define ESC            (0x01 + FLAG_EXT)  // Esc
#define TAB            (0x02 + FLAG_EXT)  // Tab
#define ENTER          (0x03 + FLAG_EXT)  // Enter
#define BACKSPACE      (0x04 + FLAG_EXT)  // BackSpace

#define GUI_L          (0x05 + FLAG_EXT)  // L GUI
#define GUI_R          (0x06 + FLAG_EXT)  // R GUI
#define APPS           (0x07 + FLAG_EXT)  // APPS

#define SHIFT_L        (0x08 + FLAG_EXT)  // L Shift
#define SHIFT_R        (0x09 + FLAG_EXT)  // R Shift
#define CTRL_L         (0x0A + FLAG_EXT)  // L Ctrl
#define CTRL_R         (0x0B + FLAG_EXT)  // R Ctrl
#define ALT_L          (0x0C + FLAG_EXT)  // L Alt
#define ALT_R          (0x0D + FLAG_EXT)  // R Alt

#define CAPS_LOCK      (0x0E + FLAG_EXT)  //
#define NUM_LOCK       (0x0F + FLAG_EXT)  //
#define SCROLL_LOCK    (0x10 + FLAG_EXT)  //

#define F1    (0x11 + FLAG_EXT)  // F1
#define F2    (0x12 + FLAG_EXT)  // F2
#define F3    (0x13 + FLAG_EXT)  // F3
#define F4    (0x14 + FLAG_EXT)  // F4
#define F5    (0x15 + FLAG_EXT)  // F5
#define F6    (0x16 + FLAG_EXT)  // F6
#define F7    (0x17 + FLAG_EXT)  // F7
#define F8    (0x18 + FLAG_EXT)  // F8
#define F9    (0x19 + FLAG_EXT)  // F9
#define F10   (0x1A + FLAG_EXT)  // F10
#define F11   (0x1B + FLAG_EXT)  // F11
#define F12   (0x1C + FLAG_EXT)  // F12

#define PRINTSCREEN  (0x1D + FLAG_EXT)  // Print Screen
#define PAUSEBREAK   (0x1E + FLAG_EXT)  // Pause/Break
#define INSERT       (0x1F + FLAG_EXT)  // Insert
#define DELETE       (0x20 + FLAG_EXT)  // Delete
#define HOME         (0x21 + FLAG_EXT)  // Home
#define END          (0x22 + FLAG_EXT)  // End
#define PAGEUP       (0x23 + FLAG_EXT)  // Page Up
#define PAGEDOWN     (0x24 + FLAG_EXT)  // Page Down
#define UP           (0x25 + FLAG_EXT)  // Up
#define DOWN         (0x26 + FLAG_EXT)  // Down
#define LEFT         (0x27 + FLAG_EXT)  // Left
#define RIGHT        (0x28 + FLAG_EXT)  // Right

#define POWER  (0x29 + FLAG_EXT)  // Power
#define SLEEP  (0x2A + FLAG_EXT)  // Sleep
#define WAKE   (0x2B + FLAG_EXT)  // Wake Up

#define PAD_SLASH  (0x2C + FLAG_EXT)  // /
#define PAD_STAR   (0x2D + FLAG_EXT)  // *
#define PAD_MINUS  (0x2E + FLAG_EXT)  // -
#define PAD_PLUS   (0x2F + FLAG_EXT)  // +
#define PAD_ENTER  (0x30 + FLAG_EXT)  // Enter
#define PAD_DOT    (0x31 + FLAG_EXT)  // .
#define PAD_0      (0x32 + FLAG_EXT)  // 0
#define PAD_1      (0x33 + FLAG_EXT)  // 1
#define PAD_2      (0x34 + FLAG_EXT)  // 2
#define PAD_3      (0x35 + FLAG_EXT)  // 3
#define PAD_4      (0x36 + FLAG_EXT)  // 4
#define PAD_5      (0x37 + FLAG_EXT)  // 5
#define PAD_6      (0x38 + FLAG_EXT)  // 6
#define PAD_7      (0x39 + FLAG_EXT)  // 7
#define PAD_8      (0x3A + FLAG_EXT)  // 8
#define PAD_9      (0x3B + FLAG_EXT)  // 9
#define PAD_UP        PAD_8      // Up
#define PAD_DOWN      PAD_2      // Down
#define PAD_LEFT      PAD_4      // Left
#define PAD_RIGHT     PAD_6      // Right
#define PAD_HOME      PAD_7      // Home
#define PAD_END       PAD_1      // End
#define PAD_PAGEUP    PAD_9      // Page Up
#define PAD_PAGEDOWN  PAD_3      // Page Down
#define PAD_INS       PAD_0      // Ins
#define PAD_MID       PAD_5      // Middle key
#define PAD_DEL       PAD_DOT    // Del


#define CK_LSHIFT               0x01
#define CK_LALT                 0x02
#define CK_LCTRL                0x04
#define CK_RSHIFT               0x10
#define CK_RALT                 0x20
#define CK_RCTRL                0x40

//
// Keyboard LEDs
//
#define LED_NUM_LOCK            2
#define LED_SCROLL_LOCK         1
#define LED_CAPS_LOCK           4

//
// Keyboard Commands
//

#define KBD_CMD_SET_LEDS        0xED    // Set keyboard leds
#define KBD_CMD_SET_RATE        0xF3    // Set typematic rate
#define KBD_CMD_ENABLE          0xF4    // Enable scanning
#define KBD_CMD_DISABLE         0xF5    // Disable scanning
#define KBD_CMD_RESET           0xFF    // Reset




#define KBD_STATUS	0x64
#define KBD_CMDS	0x64
#define KBD_DATA	0x60

//
// Keyboard Replies
//

#define KBD_REPLY_POR           0xAA    // Power on reset
#define KBD_REPLY_ACK           0xFA    // Command ACK
#define KBD_REPLY_RESEND        0xFE    // Command NACK, send command again

//
// Keyboard Status Register Bits
//

#define KBD_STAT_OBF            0x01    // Keyboard output buffer full
#define KBD_STAT_IBF            0x02    // Keyboard input buffer full
#define KBD_STAT_SELFTEST       0x04    // Self test successful
#define KBD_STAT_CMD            0x08    // Last write was a command write
#define KBD_STAT_UNLOCKED       0x10    // Zero if keyboard locked
#define KBD_STAT_MOUSE_OBF      0x20    // Mouse output buffer full
#define KBD_STAT_GTO            0x40    // General receive/xmit timeout
#define KBD_STAT_PERR           0x80    // Parity error

#define KBD_INPUT_QSIZE 512
#define KBD_INPUT_QMASK 0x7f
#define NEXT(index) (((index) + 1) & KBD_INPUT_QMASK)


extern int tty_input(struct tty *tp, int c);



////FIX ME: not test pad key, because i have no usb keyboard when writing this code,
////on my laptop it works correctly
//static u16 keymap[] = {
//		' ', ' ', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
//		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', ' ',
//		'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', ' ',
//		'/', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ' , '*', ' ',' ',' ',
//		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, NUM_LOCK, SCROLL_LOCK, HOME, UP, PAGEUP,
//		PAD_MINUS, LEFT, PAD_MID, RIGHT, PAD_PLUS, PAD_END, PAD_DOWN, PAD_PAGEDOWN, PAD_INS,
//		PAD_DEL, ' ', ' ', ' ', ' ',
//
//		' ', ' ', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', ' ', ' ',
//		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', ' ',  ' ',
//        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':','\"', '~', ' ',
//        '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', ' ', ' ', ' ', ' ', ' ',
//		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, NUM_LOCK, SCROLL_LOCK, HOME, UP, PAGEUP,
//		PAD_MINUS, LEFT, PAD_MID, RIGHT, PAD_PLUS, PAD_END, PAD_DOWN, PAD_PAGEDOWN, PAD_INS,
//		PAD_DEL, ' ', ' ', ' ', ' ',
//};


//struct kbd *g_kbd_dev = NULL;

void put_queue(struct kbd *pkbd, struct kbd_input *keycode)
{
	if (pkbd){
		if (pkbd->flags & KBD_EVENT) {
			if (NEXT(pkbd->kbd_inqtail) != pkbd->kbd_inqhead){
				pkbd->kbd_keybuf[pkbd->kbd_inqtail] = *keycode;
				pkbd->kbd_inqtail = NEXT(pkbd->kbd_inqtail);
			} else
				LOG_INFO("keybuf full");

			if (pkbd->wq){
				wakeup_waiters(pkbd->wq);
				pkbd->wq = NULL;
			}

			schedgfxisr(GFXISR_KBD);
		}
		if (pkbd->flags & KBD_CANON) {
			if (keycode->kbd_flag >> 8 & KB_UP)
				return;
			struct tty *tp = pkbd->k_tty;
			if (tp) {
				tty_input(tp, (u32)keycode->kbd_keycode);
			}
		}
	}

}

int dequeue(struct kbd *pkbd, u32 *keycode)
{
	if (pkbd) {
		if (pkbd->kbd_inqhead != pkbd->kbd_inqtail){
			*keycode = *(u32 *)&pkbd->kbd_keybuf[pkbd->kbd_inqhead];
			pkbd->kbd_inqhead = NEXT(pkbd->kbd_inqhead);
			return 0;
		}
	}

	return -1;
}

int get_key(struct kbd *pkbd, u32 *keycode)
{
	return dequeue(pkbd, keycode);
}

//void keyboard_handler(int irq, void *data, trap_frame_t *regs)
//{
//
//	uchar_t scancode = inb(0x60);
//	ushort_t keycode = 0;
//	static char caps_lock = 0;
//
//	if (scancode == 0xE0 || scancode == 0xE1){
//		return;
//	}
//
//
//
//#ifdef DEBG_KBD
//	printk("scancode:%d ",scancode);
//#endif
//	switch (scancode) {
//	case LEFTSHIFT:
//	case RIGHTSHIFT:
//		shift = 0x58;
//		break;
//	case 0x3a:				//CapsLock
//		if (caps_lock == 0)
//			caps_lock = 1;
//		else
//			caps_lock = 0;
//		break;
//	case LEFTSHIFT | 0x80:
//	case RIGHTSHIFT | 0x80:
//		shift = 0;
//		break;
//	default:
//		if (scancode < 0x57){
//			if (caps_lock){
//				keycode = keymap[scancode + (0x58 - shift)];
//			}else
//				keycode = keymap[scancode + shift];
//
//		put_queue(keycode);
//#ifdef DEBG_KBD
//			printk("%c",keycode);
//#endif
//
//
//	}
//  }
//
//}

static void update_led(u32 led_status)
{
	u32 timeout = 0x1000;
	outb(KBD_DATA, KBD_CMD_SET_LEDS);
	while (inb(KBD_STATUS) & KBD_STAT_IBF) {
		if (--timeout == 0)
			break;
	}

	outb(KBD_DATA, led_status);
}



void keyboard_handler(int irq, void *data, trap_frame_t *regs)
{
	u16 keycode = 0;
	int state;
	u8 scancode = inb(KBD_DATA);
	struct kbd *pkbd = (struct kbd *)data;
	struct kbd_input key;
	LOG_DEBG("scancode:%x", scancode);
	//g_kbd_dev should be a point cast of data.
	//if you have two keyboard devices.per devcice a struct kbd.
	if (scancode == 0xE0) {
		pkbd->is_E0 = 1;
		return;
	}
	//ctl-alt-sysrq
	if ((pkbd->ctl_keys & (CK_LCTRL | CK_LALT)) && scancode == 0xD4) {
		//dbg
		//asm int 3
		return;
	}
	//ctl-alt-del
	if ((pkbd->ctl_keys & (CK_LCTRL | CK_LALT)) && scancode == 0x53) {
		//do reboot or whatever.
		return;
	}
	//Caps lock
	if (scancode == 0x3A) {
		pkbd->led_status ^= LED_CAPS_LOCK;
		update_led(pkbd->led_status);
	}
	//Num lock
	if (scancode == 0x45) {
		pkbd->led_status ^= LED_NUM_LOCK;
		update_led(pkbd->led_status);
	}
	//Scroll lock
	if (scancode == 0x46) {
		pkbd->led_status ^= LED_SCROLL_LOCK;
		update_led(pkbd->led_status);
	}
	//ctrl keys
	if (!pkbd->is_E0 && scancode == 0x1D)
		pkbd->ctl_keys |= CK_LCTRL;
	if (!pkbd->is_E0 && scancode == (0x1D | 0x80))
		pkbd->ctl_keys &= ~CK_LCTRL;

	if (pkbd->is_E0 && scancode == 0x1D)
		pkbd->ctl_keys |= CK_RCTRL;
	if (pkbd->is_E0 && scancode == (0x1D|0x80))
		pkbd->ctl_keys &= ~CK_RCTRL;
	//shift keys
	if (scancode == 0x2A)
		pkbd->ctl_keys |= CK_LSHIFT;
	if (scancode == (0x2A | 0x80))
		pkbd->ctl_keys &= ~CK_LSHIFT;

	if (scancode == 0x36)
		pkbd->ctl_keys |= CK_RSHIFT;
	if (scancode == (0x36 | 0x80))
		pkbd->ctl_keys &= ~CK_RSHIFT;

	//alt key
	if (!pkbd->is_E0 && scancode == 0x38)
		pkbd->ctl_keys |= CK_LALT;
	if (!pkbd->is_E0 && scancode == (0x38 | 0x80))
		pkbd->ctl_keys &= ~CK_LALT;

	if (pkbd->is_E0 && scancode == 0x38)
		pkbd->ctl_keys |= CK_RALT;
	if (pkbd->is_E0 && scancode == (0x38 | 0x80))
		pkbd->ctl_keys &= ~CK_RALT;

	if ((pkbd->ctl_keys & (CK_LSHIFT|CK_RSHIFT)) && (pkbd->led_status & LED_CAPS_LOCK))
		state = KBSTATE_SHIFTCAPS;
	else if ((pkbd->ctl_keys & CK_LSHIFT) && (pkbd->ctl_keys & CK_LCTRL))
		state = KBSTATE_SHIFTCTRL;
	else if (pkbd->ctl_keys & (CK_LSHIFT|CK_RSHIFT))
		state = KBSTATE_SHIFT;
	else if (pkbd->ctl_keys & (CK_LCTRL|CK_RCTRL))
		state = KBSTATE_CTRL;
	else if (pkbd->ctl_keys & CK_LALT)
		state = KBSTATE_ALT;
	else if (pkbd->ctl_keys & CK_RALT)
		state = KBSTATE_ALTGR;
	else if ((pkbd->ctl_keys & (CK_LSHIFT|CK_RSHIFT)) && (pkbd->led_status & LED_NUM_LOCK))
		state = KBSTATE_SHIFTNUM;
	else if (pkbd->led_status & LED_CAPS_LOCK)
		state = KBSTATE_CAPSLOCK;
	else if (pkbd->led_status & LED_NUM_LOCK)
		state = KBSTATE_NUMLOCK;
	else if (pkbd->ctl_keys == 0)
		state = KBSTATE_NORMAL;

	if (scancode < MAX_SCANCODES) {
		key.kbd_flag = KB_DOWN << 8 | state;
	} else {
		scancode -= 0x80;
		key.kbd_flag = KB_UP << 8 | state;
	}

	if (pkbd->is_E0)
		keycode = extkeys[scancode][state];
	else
		keycode = pkbd->kt->keymap[scancode][state];

	key.kbd_keycode = keycode;
//	if ( key.kbd_flag >> 8 & KB_DOWN)
//		LOG_INFO("key down:%x", key.kbd_keycode);
//	else
//		LOG_DEBG("key up:%x", keycode);
	put_queue(pkbd, &key);
	pkbd->is_E0 = 0;
}


int kb_open(dev_t *dev)
{
	LOG_INFO("in");
	struct kbd *pkbd = (struct kbd *)dev->driver_data;
	if (pkbd->flags & KBD_ENABLED)
		dev->inuse++;
	else
		return -ENODEV;

	//if a tty is attached on us
//	if (pkbd->k_tty) {
//		struct tty *tp = pkbd->k_tty;
//		if (tp->t_termios.c_lflag & ICANON)
//			pkbd->flags |= ICANON;
//	}


//	if (dev->driver_data == NULL) {
//		dev->driver_data = kmalloc(sizeof (key_queue_t));
//		if (dev->driver_data == NULL){
//			//LOG_INFO("malloc failed:%#x", dev->driver_data);
//			return -ENOMEM;
//		}
//		memset(dev->driver_data, 0, sizeof(key_queue_t));
//		g_kb_dev = dev;
//		//LOG_INFO("%#x", g_kb_dev);
//	}
	//LOG_INFO("driver_data:%#x", dev->driver_data);
	LOG_INFO("end");
	return 0;
}

int kb_read(dev_t *dev, void *buf, size_t size)
{
	struct kbd *pkbd = (struct kbd *)dev->driver_data;
	u32 *p = (u32*)buf;
	u32 keycode;
	int ret = 0;
	if (!pkbd)
		return -EINVAL;

	while ((get_key(pkbd, &keycode) == 0) && (size > 0)) {
		*p++ = keycode;
		size -= 4;
		ret++;
	}
	return ret;

}

int kb_write(dev_t *dev, void *buf, size_t size)
{
	return -ENOSYS;
}

int kb_close(dev_t *dev)
{
	dev->inuse--;
	return 0;
}

int kbd_add_waiter(dev_t *dev, wait_queue_head_t *wq)
{
	struct kbd *pkbd = (struct kbd *)dev->driver_data;
	if (pkbd->wq == NULL)
		pkbd->wq = wq;
	return 0;
}


int kb_io_request(io_req_t *req)
{
	int ret = -EINVAL;
	if (req){
		if (req->flag & IO_TARGET_CHAR){
			switch(req->io_req_type){
			case IO_READ:
				ret = kb_read(req->dev, req->buf, req->_u.c_req.nr_bytes);
				if (ret < 0){
					req->io_state = IO_ERROR;
					req->error = ret;
					return ret;
				}
				if (ret == 0){
					req->io_state = IO_PENDING;
					req->error = -EAGAIN;
					return ret;
				}
				break;
			case IO_WRITE:
				ret = kb_write(req->dev, req->buf, req->_u.c_req.nr_bytes);
				if (ret < 0){
					req->io_state = IO_ERROR;
					req->error = ret;
					return ret;
				}
				break;
			default:
				ret = -ENOSYS;
				req->error = ret;
				req->io_state = IO_ERROR;
				return ret;
			}
		}

		req->io_state = IO_COMPLETED;
		req->error = ret;
	}

	return ret;
}



static struct dev_ops kb_ops = {
	.open = kb_open,
	.read = kb_read,
	.write = kb_write,
	.close = kb_close,
	.io_request = kb_io_request,
};

void keyboard_init(void)
{
	struct kbd *pkbd = kzalloc(sizeof(*pkbd));
	assert(pkbd != NULL);

	extern u32 graphic;
	if (graphic)
		pkbd->flags = (KBD_EVENT | KBD_ENABLED);
	else
		pkbd->flags = (KBD_CANON | KBD_ENABLED);
	pkbd->kbd_keybuf = kzalloc(KBD_INPUT_QSIZE);
	assert(pkbd->kbd_keybuf != NULL);

	pkbd->kt = &us_keymap;

	register_device("keyboard", &kb_ops, MKDEVID(1,0), (void *)pkbd);
	register_irq(1, (u32)keyboard_handler, (void *)pkbd, IRQ_REPLACE);
	enable_irq(1);
}
