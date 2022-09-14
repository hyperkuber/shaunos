/*
 * console.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <kernel/kernel.h>
#include <kernel/console.h>
#include <string.h>
#include <asm/io.h>
#include <unistd.h>
#include <asm/system.h>
#include <kernel/page.h>
#include <kernel/vfs.h>
#include <kernel/stat.h>
#include <kernel/assert.h>
#include <kernel/time.h>
#include <kernel/malloc.h>
#include <kernel/io_dev.h>
#include <kernel/keyboard.h>

#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

#define _POSIX_VDISABLE	((unsigned char)'\377')

#ifndef _POSIX_SOURCE
#define CCEQ(val, c)	(c == val ? val != _POSIX_VDISABLE : 0)
#endif

/* Is 'c' a line delimiter ("break" character)? */
#define	TTBREAKC(c)							\
	((c) == '\n' || (((c) == cc[VEOF] ||				\
	(c) == cc[VEOL] || (c) == cc[VEOL2]) && (c) != _POSIX_VDISABLE))

#define TTY_CHARMASK 0xff

struct con_obj curr_con;

#define DEFAULT_ATTRIBUTE ATTRIB(BLACK, GRAY)

void set_cursor();

extern struct timespec xtime;

#define CURRENT_TIME_SEC (xtime.ts_sec)


static int color_map[8] = {0, 4, 2, 6, 1, 5, 3, 7};

static inline int detect_video_type(void)
{
    char c = (*(volatile ushort_t*)0x410)&0x30;

    //c can be 0x00 or 0x20 for colour, 0x30 for mono.
    return (c==0x30);
}

static inline unsigned int get_video_membase(void)
{
	return ((detect_video_type() == 0) ? (0xb8000 + __PAGE_OFFSET) : (0xb0000 + __PAGE_OFFSET));
}


void setattr(unsigned char attr)
{
	curr_con.attr = attr;
}

static void scroll_up(void)
{
    uint_t* v;
    int i, n = (curr_con.rows -1) * curr_con.cols * 2 / 4;
    uint_t fill = 0x00200020 | (curr_con.attr<<24) | (curr_con.attr<<8);

	memcpy((void *)curr_con.video_membase, (void *)(curr_con.video_membase + curr_con.cols * 2),
			(curr_con.rows - 1) * curr_con.cols * 2);

    for (v = (uint_t*)curr_con.video_membase + n, i = 0; i < (curr_con.cols * 2 / 4); ++i)
    	*v++ = fill;
}

static void scroll_down()
{

}


static void fill_to_line_end(void)
{
	int i = curr_con.cols - curr_con.col;

	uchar_t *p = (uchar_t *)(curr_con.video_membase + curr_con.row * (curr_con.cols * 2) +
			curr_con.col * 2);

	while (i-- > 0){
		*p++ = ' ';
		*p++ = curr_con.attr;
	}
}

static void new_line(void)
{
    ++curr_con.row;
    //curr_con.col = 0; //printf("abcd\n123"), 123 should not in a new line head pos.
    if (curr_con.row == curr_con.rows) {
    	scroll_up();
    	curr_con.row = curr_con.rows - 1;
    }
}

void set_cursor(void)
{
	uint_t pos = curr_con.row * (curr_con.cols * 2) + curr_con.col * 2;
	outb(curr_con.video_ctrler_reg, 14);
	outb(curr_con.video_ctrler_val, (pos>>9) & 0xFF);
	outb(curr_con.video_ctrler_reg ,15);
	outb(curr_con.video_ctrler_val, pos>>1 & 0xFF);
}
void cls()
{
	int i;
	u8 *p = (u8 *)curr_con.video_membase;
	for (i=0; i< curr_con.rows * curr_con.cols * 2 ; i++ ) {
		*p++ = ' ';
		*p++ = DEFAULT_ATTRIBUTE;
	}
	curr_con.pos = 0;
	curr_con.row = curr_con.col = 0;
	// cursor should be updated outside of cls()
}
void gotoxy(uint_t row, uint_t col)
{
	if ( row < 0 || row >= curr_con.rows || col < 0 || col >= curr_con.cols)
		return;
	curr_con.col = col;
	curr_con.row = row;
	set_cursor();
}
void __put_char(int c)
{
    uchar_t* p = (uchar_t *)(curr_con.video_membase +
    		curr_con.row *(curr_con.cols * 2) + curr_con.col * 2);

    *p++ = (uchar_t) c;
    *p = curr_con.attr;

    if (curr_con.col  < curr_con.cols - 1)
    	++curr_con.col;
    else {
    	curr_con.col = 0;
    	new_line();
    }

}

void put_char(int c)
{
	int ns;
	switch (c) {
	case 0:
		break;
	case '\n':
		fill_to_line_end();
		curr_con.col = 0; // '\n' should move current cursor to line head?
		new_line();
		break;
	case '\r':
		curr_con.col = 0;
		break;
	case '\t':
		ns = 8 - (curr_con.col % 8);
		while (ns-- > 0)
			__put_char(' ');
		break;
	case '\b':
		if (curr_con.col != 0) {
			curr_con.col -= 1;
			__put_char(' ');
			curr_con.col--;
		}
		break;
	case 12://Formfeed
		cls();
		break;
	case 27://Escape
		curr_con.video_state = 1;
		break;
	default:
		__put_char(c);

	}
}

void process_sequence(int x, int y, char ch)
{
	switch(ch) {
	case 'H': {
		int row, col;

		row = x;
		col = y;

		if (row < 1) row = 1;
		if (row > curr_con.rows ) row = curr_con.rows;
		if (col < 1) col = 1;
		if (col > curr_con.cols) col = curr_con.cols;
		gotoxy(row, col);
		break;
		}
	case 'J':
		if (x == 1) {
			//clear screen form current position
		} else {
			cls();
		}
		break;
	case 'K':
		fill_to_line_end();
		break;
	case 'm':
		if (x >= 30 && x <= 37) {
			// Foreground color
			curr_con.attr = color_map[x-30] + (curr_con.attr & 0xF8);
		} else if (x >= 40 &&  x <= 47) {
			//Background color
			curr_con.attr = (color_map[x-40] << 4) + (curr_con.attr & 0x8F);
		} else if (x == 1) {
			//High intensity foreground
			curr_con.attr |= 8;
		} else if (x == 2) {
			//Low intensity foreground
			curr_con.attr &= ~8;
		} else if (x == 5) {
			//High intensity background
			curr_con.attr |= 128;
		} else if (x == 6) {
			//Low intensity background
			curr_con.attr &= ~128;
		} else if (x == 7) {
			//Reverse
			curr_con.attr = ((curr_con.attr & 0xF0) >> 4) + ((curr_con.attr & 0x0F) << 4);
		} else if (x == 8) {
			//Invisible make foreground match background
			curr_con.attr = ((curr_con.attr & 0xF0) >> 4) + (curr_con.attr & 0xF0);
		} else {
			curr_con.attr = DEFAULT_ATTRIBUTE;
		}
		break;
	case 'A':
		//Cursor up
		while (x-- > 0) {
			if (curr_con.row >= 1)
				curr_con.row--;
			else
				break;
		}
		break;
	case 'B':
		//Cursor down
		while (x-- > 0) {
			if (curr_con.row < curr_con.rows)
				curr_con.row++;
			else
				break;
		}
		break;
	case 'C':
		//Cursor right
		curr_con.col += x*2;
		if (curr_con.col > curr_con.cols)
			curr_con.col = curr_con.cols -1;
		break;
	case 'D':
		//Cursor left
		curr_con.col -= x*2;
		if (curr_con.col < 0)
			curr_con.col = 0;
		break;
	case 'L':
		//insert line
		while (x-- > 0) {
			new_line();
		}
		break;
	case 'M':
		//delete line
		while (x-- > 0) {
			//clear_line()
		}
		break;
	case '@':
		//insert character
		while (x-- > 0) {
			//
		}
		break;
	case 'P':
		//delete character
		while (x-- > 0) {
			//
		}
		break;
	case 's':
		//save cursor
	case 'u':
		//restore cursor
		break;
	case 'h':
		//enable line wrap
		if (x == 7)
			curr_con.line_wrap = 1;
		break;
	case 'l':
		//disable line wrap
		if (x == 7)
			curr_con.line_wrap = 0;
		break;
	}
}


int process_multichar(int state, char ch)
{
	static int x, y;
	switch (state) {
	case 1: //we got Escape
		switch (ch) {
		case '[': //Extended sequence
			return 2;
		case 'P': //Cursor down a line
			curr_con.row++;
			if (curr_con.row >= curr_con.rows) {
				//
				curr_con.row = curr_con.rows -1;
			}
			return 0;
		case 'K': //Cursor left
			if (curr_con.col > 0)
				curr_con.col -= 2;
			return 0;
		case 'H': //Cursor up
			if (curr_con.row > 0)
				curr_con.row--;
			return 0;
		case 'D': //Scroll forward
			scroll_up();
			return 0;
		case 'M': //Scroll down
			scroll_down();
			return 0;
		case 'G': //Cursor home
			curr_con.row = curr_con.col = 0;
			return 0;
		case '(': //Extended char set
			return 5;
		case 'c' ://reset
			curr_con.row = curr_con.col = 0;
			curr_con.attr = DEFAULT_ATTRIBUTE;
			return 0;
		default:
			return 0;
		}
	case 2:	//Esc-[
		if (ch == '?')
			return 3;
		if (ch >= '0' && ch <= '9') {
			x = ch - '0';
			return 3;
		}

		if (ch == 's' || ch == 'u') {
			process_sequence(0, 0, ch);
		} else if (ch == 'r' || ch == 'm') {
			process_sequence(0, 0, ch);
		} else {
			process_sequence(1, 1, ch);
		}
		return 0;
	case 3:
		if (ch >= '0' && ch <= '9') {
			x = x*10 + (ch - '0');
			return 3;
		}

		if (ch == ';') {
			y = 0;
			return 4;
		}
		process_sequence(x, 1, ch);
		return 0;
	case 4: //Esc[0-9;
		if (ch >= '0' && ch <= '9') {
			y = y*10 + (ch - '0');
			return 4;
		}
		process_sequence(x, y, ch);
		return 0;
	case 5: //Esc(
		//ignore, fallow-through
	default:
		return 0;
	}
}



void write_string( const char *s , u32 len)
{
	int ch;
	char *end;

	if (!s)
		return;

	end = (char *)s + len;

	while (s < end) {
		ch = *s++;
		if (curr_con.video_state > 0) {
			curr_con.video_state = process_multichar(curr_con.video_state, ch);
			continue;
		}
		put_char(ch);
	}
    set_cursor();
}

int con_open(struct vds *dev)
{
	dev_t *kbdev;
	struct kbd *pkbd;
	struct tty *tp = (struct tty *)dev->driver_data;

	LOG_INFO("in");

	int retval = open_device("keyboard", &kbdev);
	if (retval < 0) {
		LOG_ERROR("open keyboard failed");
		return retval;
	}

	pkbd = (struct kbd *)kbdev->driver_data;
	if (pkbd->k_tty == NULL)
		pkbd->k_tty = tp;

	kbdev->ops->close(kbdev);

	dev->inuse++;
	LOG_INFO("end");
	return retval;
}
int con_close(struct vds *dev)
{
	dev->inuse--;
	return 0;
}

int con_read(struct vds *dev, void *buf, size_t size)
{
	int ret = 0;
	return ret;
}

int con_write(struct vds *dev, void *buf, size_t size)
{
	write_string((char *)buf, size);
	return 0;
}

int con_io_request(io_req_t *req)
{
	int ret;
	if (req){
		if (req->flag & IO_TARGET_CHAR){
			switch(req->io_req_type){
			case IO_READ:
				ret = con_read(req->dev, (char *)req->buf + req->_u.c_req.offset, req->_u.c_req.nr_bytes);
				if (ret < 0){
					LOG_ERROR("con_read failed");
					req->error = ret;
					req->io_state = IO_ERROR;
					break;
				}

				if (ret == 0){
					req->error = -EAGAIN;
					req->io_state = IO_PENDING;
					break;
				}

				if (ret <= req->_u.c_req.nr_bytes){
					//for terminal, when we type Enter key, console read should return
					if (((char *)req->buf)[req->_u.c_req.offset + ret -1] == '\n'){
						req->error = req->_u.c_req.offset + ret;
						req->io_state = IO_COMPLETED;
						//replace '\n' with '\0'
						((char *)req->buf)[req->_u.c_req.offset + ret -1] = 0;
						//LOG_INFO("io completed, buf:%s", req->buf);
						break;
					}

					req->_u.c_req.offset += ret;
					if (req->_u.c_req.offset >= req->_u.c_req.nr_bytes){
						req->io_state = IO_COMPLETED;
						req->error = req->_u.c_req.offset;
						break;
					}
					req->io_state = IO_PENDING;
					req->error = ret;
				}
				break;
			case IO_WRITE:
				ret = con_write(req->dev, req->buf, req->_u.c_req.nr_bytes);
				if (ret < 0){
					req->io_state = IO_ERROR;
					req->error = ret;
					break;
				}

				req->io_state = IO_COMPLETED;
				req->error = req->_u.c_req.nr_bytes;
				break;
			case IO_COMMAND:
			default:
				req->error = -ENOSYS;
				req->io_state = IO_ERROR;
				break;
			}
		} else {
			req->error = -ENOSYS;
			req->io_state = IO_ERROR;
		}
	}

	return req->error;
}


static struct dev_ops con_ops = {
	.open = con_open,
	.read = con_read,
	.write = con_write,
	.close = con_close,
	.io_request = con_io_request,
};





int console_init(void)
{
	int i;
	curr_con.video_membase = get_video_membase();
	curr_con.attr = DEFAULT_ATTRIBUTE;
	curr_con.pos = (u8 *)curr_con.video_membase;
	curr_con.rows = readaddr8(0x0,0x484) + 1;
	curr_con.cols = readaddr16(0x0,0x44a);
	//memset((void *)curr_con.pos, 0x07, curr_con.cols * curr_con.rows * 2);
	for (i=0; i< curr_con.rows * curr_con.cols * 2 ; i+=2 )
		*(curr_con.pos + i) = ' ';

	curr_con.row = curr_con.col = 0;
	curr_con.video_ctrler_reg = 0x3d4;
	curr_con.video_ctrler_val = 0x3d5;
	set_cursor();

	return 0;

}

#define NEXT(x)	(((x)+1) & 0x1FF)

int tty_getc(struct cbuf *p)
{
	int retval = -1;
	if (p->c_chead != p->c_ctail && p->c_cc > 0) {
		retval = p->c_cbuf[p->c_chead];
		p->c_chead = NEXT(p->c_chead);
		p->c_cc--;
	}
	return retval;
}

int tty_putc(struct cbuf *p, int c)
{
	if (NEXT(p->c_ctail) != p->c_chead) {
		p->c_cbuf[p->c_ctail] = (u8)c;
		p->c_ctail = NEXT(p->c_ctail);
		p->c_cc++;
	}

	return 0;
}
int tty_erc(struct cbuf *p)
{
	if (p->c_cc > 0) {
		if (p->c_ctail == 0)
			p->c_ctail = 255;
		p->c_cbuf[p->c_ctail] = 0;
		--p->c_ctail;
		--p->c_cc;
	}
	return 0;
}


int tty_read(file_t *file, void *buf, ulong_t nbytes)
{
	dev_t *dev;
	struct tty *tp;
	int c, cnt;
	char *cp = (char *)buf;
	dev = file->f_vnode->v_mp->dev;
	cnt = 0;
	tp = (struct tty *)dev->driver_data;

	if (tp->t_canq.c_cc <= 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EWOULDBLOCK;

		io_wait(&tp->t_wq);
	}

	while ((c = tty_getc(&tp->t_canq)) >= 0) {
		//handle eof

		*cp++ = (u8)c;
		cnt++;
		if (cnt == nbytes)
			break;
		if ( c  == '\n')
			break;
	}

	return cnt;
}

int tty_write(file_t *file, void *buf, ulong_t nbytes)
{
	int retval;
	io_req_t *req = io_create_req_chr(file->f_vnode->v_mp->dev, IO_WRITE, nbytes, buf);

	if (req){
		if (file->f_flags & O_NONBLOCK){
			req->flag |= IO_AYSNC;
		}

		retval = io_request(req);
		if (retval < 0){
			retval = -EIO;
			goto END;
		}
		retval = req->error;
		goto END;
	}
	//retval = -ENOMEM;
END:
	io_free_req(req);
	return retval;
}
int tty_fstat(file_t *file, struct stat *buf)
{
	buf->st_dev = file->f_vnode->v_device;
	buf->st_blksize = 1024;
	return 0;
}
int tty_seek(file_t *file, off_t pos, int whence)
{
	int row , col;
	if (whence == SEEK_SET){
		goto END;
	} else if (whence == SEEK_CUR){
		pos += (curr_con.row * curr_con.col + curr_con.col);
		goto END;
	} else
		return -ENOSYS;
END:
	row = pos / curr_con.cols;
	col = pos % curr_con.rows;
	gotoxy(row, col);
	return 0;
}
int tty_close(file_t *file)
{
	if (!file)
		return -EINVAL;

	if (file->f_refcnt > 0)
		file->f_refcnt--;

	if (file->f_refcnt == 0){
		if (file->f_vnode)
			if (--file->f_vnode->v_refcnt == 0) {
				file->f_vnode->v_mp->dev->ops->close(file->f_vnode->v_mp->dev);
				kfree(file->f_vnode);
			}
		kfree(file);
	}
	return 0;
}

static struct file_ops tty_file_ops = {
		.read = tty_read,
		.write = tty_write,
		.fstat = tty_fstat,
		.seek = tty_seek,
		.close = tty_close
};


void tty_wakeup(wait_queue_head_t *wq)
{
	io_wakeup(wq);
}

int tty_output(struct tty *tp, int c)
{

	CLR(c, ~TTY_CHARMASK);

//	if (c == '\b') {
//		tty_erc(&tp->t_outq);
//		return 0;
//	}

	tty_putc(&tp->t_outq, c);

	return 0;
}

int tty_start(struct tty *tp)
{
	int c;
	dev_t *dev = tp->t_dev;

	while ((c = tty_getc(&tp->t_outq)) >= 0)
		dev->ops->write(dev, (void *)&c, 1);
	return 0;
}

/*
 * Echo a typed character to the terminal.
 */
void
tty_echo(struct tty *tp, int c)
{

	if (!ISSET(tp->t_state, TS_CNTTB))
		CLR(tp->t_lflag, FLUSHO);
	if ((!ISSET(tp->t_lflag, ECHO) &&
	    (!ISSET(tp->t_lflag, ECHONL) || c == '\n')) ||
	    ISSET(tp->t_lflag, EXTPROC))
		return;
	if (ISSET(tp->t_lflag, ECHOCTL) &&
	    ((ISSET(c, TTY_CHARMASK) <= 037 && c != '\t' && c != '\n') ||
	    ISSET(c, TTY_CHARMASK) == 0177)) {
		(void)tty_output(tp,'^');
		CLR(c, ~TTY_CHARMASK);
		if (c == 0177)
			c = '?';
		else
			c += 'A' - 1;
	}
	tty_output(tp, c);
}

/*
 * Flush tty read and/or write queues, notifying anyone waiting.
 */
void
tty_flush(struct tty *tp, int rw)
{
	if (rw & FREAD) {
		CLR(tp->t_state, TS_LOCAL);
		//tty_wakeup(&tp->t_wq);
	}
	if (rw & FWRITE) {
		CLR(tp->t_state, TS_TTSTOP);
		//tty_wakeup(&tp->t_wq);
	}
}


/*
 * we have a lot of things to do here.
 */
int tty_input(struct tty *tp, int c)
{
	u32 lflag, iflag;
	lflag = tp->t_lflag;
	iflag = tp->t_iflag;
	u8 *cc = tp->t_cc;

	if (!ISSET(lflag, EXTPROC)) {

		if (ISSET(lflag, ISIG)) {
			if (CCEQ(cc[VINTR], c) || CCEQ(cc[VQUIT], c)) {
				if (!ISSET(lflag, NOFLSH))
					tty_flush(tp, FREAD|FWRITE);
				tty_echo(tp, c);
				//send sigint
				goto end;
			}
			if (CCEQ(cc[VSUSP], c)) {
				if (!ISSET(lflag, NOFLSH))
					tty_flush(tp, FREAD);
				tty_echo(tp, c);
				//pt_stop();
				goto end;
			}
		}

		if (ISSET(iflag, IXON)) {
			if (CCEQ(cc[VSTOP], c)) {
				if (!ISSET(tp->t_state, TS_TTSTOP))
					SET(tp->t_state, TS_TTSTOP);

				if (!CCEQ(cc[VSTART],c))
					return 0;
			}
			if (CCEQ(cc[VSTART], c))
				goto restart;

		}

		if (c == '\r') {
			if (ISSET(iflag, IGNCR))
				goto end;
			else if (ISSET(iflag, ICRNL))
				c = '\n';
		} else if (c == '\n' && ISSET(iflag, INLCR))
			c = '\r';
	}

	if (ISSET(lflag, ICANON)) {
		tp->t_cancc++;
#if 0
		if (CCEQ(cc[VERASE], c)) {
			tty_erc(&tp->t_canq);
			goto echo;
		}

		if (CCEQ(cc[VKILL], c)) {
			goto end;
		}

		if (CCEQ(cc[VWERASE], c)) {
			goto end;
		}

		if (CCEQ(cc[VREPRINT], c)) {
			goto end;
		}

		if (CCEQ(cc[VSTATUS], c)) {
			goto end;
		}
#endif

		if (c == '\b') {
			tty_erc(&tp->t_canq);
			tp->t_cancc--;
			goto echo;
		}

		tty_putc(&tp->t_canq, c);

		if (TTBREAKC(c))
			tty_wakeup(&tp->t_wq);
echo:
		if (ISSET(lflag, ECHO)) {
			tty_output(tp, c);
		}

	}
restart:
	CLR(tp->t_state, TS_TTSTOP);
end:
	return tty_start(tp);
}



int mp_open(struct vnode *pv, const char *path, int mode, file_t **pfile)
{
	file_t *file = vfs_alloc_file();
	if (!file)
		return -ENOMEM;
	file->f_refcnt++;
	file->f_ops = &tty_file_ops;
	file->f_vnode = pv;

	switch (mode & O_ACCMODE){
	case O_RDONLY:
		file->f_flags |= FREAD;
		break;
	case O_WRONLY:
		file->f_flags |= FWRITE;
		break;
	case O_RDWR:
		file->f_flags |= (FREAD | FWRITE);
		break;
	}
	//file->f_flags = mode;
	pv->v_refcnt++;
	file->f_type = (S_IFCHR | S_IRWXU);
	*pfile = file;
	return 0;
}


static struct mount_point_operations mount_ops = {
		.lookup = NULL,
		.open = mp_open,
		.create = NULL,
		.stat = NULL,
		.sync = NULL,
		.delete = NULL

};


int tty_mount(vnode_t *vnode)
{

	vnode->v_atime = vnode->v_ctime = vnode->v_mtime = CURRENT_TIME_SEC;
	vnode->v_mode = S_IRUSR | S_IWUSR;
	vnode->v_mp->fsdata = (void *)vnode;		//file system set it to fs_t, dev set it to vnode
	vnode->v_mp->mp_ops = &mount_ops;

	return 0;

}

static struct file_system_ops tty_fs_ops = {
	.format = NULL,
	.mount = tty_mount,
};

static vnode_t *tty_vnode;

void console_fs_init()
{
	struct tty *tp = kzalloc(sizeof(*tp));
	tp->t_con = &curr_con;
	tp->t_termios.c_lflag = (ICANON | ECHO);
	tp->t_canq.c_cbuf = kzalloc(TTY_CBUF_SIZE);
	assert(tp->t_canq.c_cbuf != NULL);
	tp->t_outq.c_cbuf = kzalloc(TTY_CBUF_SIZE);
	assert(tp->t_outq.c_cbuf != NULL);

	list_init(&tp->t_wq.waiter);

	tp->t_iflag = ICRNL;
	tp->t_oflag = (OPOST|ONLCR);
	tp->t_cflag = 0;
	tp->t_lflag = (IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE);
	memcpy((void *)tp->t_cc, INIT_C_CC, NCCS);

	register_device("console", &con_ops, 1, (void *)tp);
	register_filesystem("tty_fs", &tty_fs_ops);
	if (vfs_mount("console", "/dev/tty", "tty_fs", &tty_vnode) < 0) {
		LOG_ERROR("mount console failed");
		while (1);
	}
	tp->t_dev = tty_vnode->v_mp->dev;
}






