/*
 * console.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#ifndef SHAUN_CONSOLE_H_
#define SHAUN_CONSOLE_H_


#include <kernel/types.h>

#include <kernel/termios.h>
#include <kernel/io_dev.h>


#define BLACK   0
#define BLUE    1
#define GREEN   2
#define CYAN    3
#define RED     4
#define MAGENTA 5
#define AMBER   6
#define GRAY    7
#define BRIGHT  8
#define ATTRIB(bg,fg) ((fg)|((bg)<<4))




struct con_obj {
	u8 *pos;
	u8 attr;
	u32 video_membase;
	u32 rows,cols;
	u32 row,col;
	u16 video_ctrler_reg;
	u16 video_ctrler_val;
	u8 video_state;
	u8 line_wrap;
};


struct cbuf {
	int c_chead;
	int c_ctail;
	int c_cc;
	s8 *c_cbuf;
};

struct tty {
	struct cbuf t_rawq;
	u32 t_raqcc;
	struct cbuf t_canq;
	u32 t_cancc;
	struct cbuf t_outq;
	u32 t_outcc;
	u32 t_state;
	u32 t_flags;
	struct termios t_termios;
	struct con_obj *t_con;
	dev_t *t_dev;
	wait_queue_head_t t_wq;
};

#define	t_cc		t_termios.c_cc
#define	t_cflag		t_termios.c_cflag
#define	t_iflag		t_termios.c_iflag
#define	t_ispeed	t_termios.c_ispeed
#define	t_lflag		t_termios.c_lflag
#define	t_min		t_termios.c_min
#define	t_oflag		t_termios.c_oflag
#define	t_ospeed	t_termios.c_ospeed
#define	t_time		t_termios.c_time

#define TTY_CBUF_SIZE	512

/* These flags are kept in t_state. */
#define	TS_ASLEEP	0x00001		/* Process waiting for tty. */
#define	TS_ASYNC	0x00002		/* Tty in async I/O mode. */
#define	TS_BUSY		0x00004		/* Draining output. */
#define	TS_CARR_ON	0x00008		/* Carrier is present. */
#define	TS_FLUSH	0x00010		/* Outq has been flushed during DMA. */
#define	TS_ISOPEN	0x00020		/* Open has completed. */
#define	TS_TBLOCK	0x00040		/* Further input blocked. */
#define	TS_TIMEOUT	0x00080		/* Wait for output char processing. */
#define	TS_TTSTOP	0x00100		/* Output paused. */
#define	TS_WOPEN	0x00200		/* Open in progress. */
#define	TS_XCLUDE	0x00400		/* Tty requires exclusivity. */

/* State for intra-line fancy editing work. */
#define	TS_BKSL		0x00800		/* State for lowercase \ work. */
#define	TS_CNTTB	0x01000		/* Counting tab width, ignore FLUSHO. */
#define	TS_ERASE	0x02000		/* Within a \.../ for PRTRUB. */
#define	TS_LNCH		0x04000		/* Next character is literal. */
#define	TS_TYPEN	0x08000		/* Retyping suspended input (PENDIN). */
#define	TS_LOCAL	(TS_BKSL | TS_CNTTB | TS_ERASE | TS_LNCH | TS_TYPEN)


int console_init(void);
void console_fs_init();
void gotoxy(uint_t row, uint_t col);



#endif /* SHAUN_CONSOLE_H_ */
