/*
 * termios.h
 *
 *  Created on: Dec 30, 2013
 *      Author: root
 */

#ifndef TERMIOS_H_
#define TERMIOS_H_


#include <kernel/types.h>





/*
 * Input flags - software input processing
 */
#define	IGNBRK		0x00000001	/* ignore BREAK condition */
#define	BRKINT		0x00000002	/* map BREAK to SIGINTR */
#define	IGNPAR		0x00000004	/* ignore (discard) parity errors */
#define	PARMRK		0x00000008	/* mark parity and framing errors */
#define	INPCK		0x00000010	/* enable checking of parity errors */
#define	ISTRIP		0x00000020	/* strip 8th bit off chars */
#define	INLCR		0x00000040	/* map NL into CR */
#define	IGNCR		0x00000080	/* ignore CR */
#define	ICRNL		0x00000100	/* map CR to NL (ala CRMOD) */
#define	IXON		0x00000200	/* enable output flow control */
#define	IXOFF		0x00000400	/* enable input flow control */
#ifndef _POSIX_SOURCE
#define	IXANY		0x00000800	/* any char will restart after stop */
#define IMAXBEL		0x00002000	/* ring bell on input queue full */
#endif  /*_POSIX_SOURCE */

/*
 * Output flags - software output processing
 */
#define	OPOST		0x00000001	/* enable following output processing */
#ifndef _POSIX_SOURCE
#define ONLCR		0x00000002	/* map NL to CR-NL (ala CRMOD) */
#define OXTABS		0x00000004	/* expand tabs to spaces */
#define ONOEOT		0x00000008	/* discard EOT's (^D) on output) */
#endif  /*_POSIX_SOURCE */

/*
 * Control flags - hardware control of terminal
 */
#ifndef _POSIX_SOURCE
#define	CIGNORE		0x00000001	/* ignore control flags */
#endif
#define CSIZE		0x00000300	/* character size mask */
#define     CS5		    0x00000000	    /* 5 bits (pseudo) */
#define     CS6		    0x00000100	    /* 6 bits */
#define     CS7		    0x00000200	    /* 7 bits */
#define     CS8		    0x00000300	    /* 8 bits */
#define CSTOPB		0x00000400	/* send 2 stop bits */
#define CREAD		0x00000800	/* enable receiver */
#define PARENB		0x00001000	/* parity enable */
#define PARODD		0x00002000	/* odd parity, else even */
#define HUPCL		0x00004000	/* hang up on last close */
#define CLOCAL		0x00008000	/* ignore modem status lines */
#ifndef _POSIX_SOURCE
#define CCTS_OFLOW	0x00010000	/* CTS flow control of output */
#define CRTSCTS		CCTS_OFLOW	/* ??? */
#define CRTS_IFLOW	0x00020000	/* RTS flow control of input */
#define	MDMBUF		0x00100000	/* flow control output via Carrier */
#endif


/*
 * "Local" flags - dumping ground for other state
 *
 * Warning: some flags in this structure begin with
 * the letter "I" and look like they belong in the
 * input flag.
 */

#ifndef _POSIX_SOURCE
#define	ECHOKE		0x00000001	/* visual erase for line kill */
#endif  /*_POSIX_SOURCE */
#define	ECHOE		0x00000002	/* visually erase chars */
#define	ECHOK		0x00000004	/* echo NL after line kill */
#define ECHO		0x00000008	/* enable echoing */
#define	ECHONL		0x00000010	/* echo NL even if ECHO is off */
#ifndef _POSIX_SOURCE
#define	ECHOPRT		0x00000020	/* visual erase mode for hardcopy */
#define ECHOCTL  	0x00000040	/* echo control chars as ^(Char) */
#endif  /*_POSIX_SOURCE */
#define	ISIG		0x00000080	/* enable signals INTR, QUIT, [D]SUSP */
#define	ICANON		0x00000100	/* canonicalize input lines */
#ifndef _POSIX_SOURCE
#define ALTWERASE	0x00000200	/* use alternate WERASE algorithm */
#endif  /*_POSIX_SOURCE */
#define	IEXTEN		0x00000400	/* enable DISCARD and LNEXT */
#define EXTPROC         0x00000800      /* external processing */
#define TOSTOP		0x00400000	/* stop background jobs from output */
#ifndef _POSIX_SOURCE
#define FLUSHO		0x00800000	/* output being flushed (state) */
#define	NOKERNINFO	0x02000000	/* no kernel output from VSTATUS */
#define PENDIN		0x20000000	/* XXX retype pending input (state) */
#endif  /*_POSIX_SOURCE */
#define	NOFLSH		0x80000000	/* don't flush after interrupt */

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16


#define NCCS	17
struct termios {
	u32 c_iflag;
	u32 c_oflag;
	u32 c_cflag;
	u32 c_lflag;
	u8 c_cc[NCCS];
	s32 c_ispeed;
	s32 c_ospeed;
};

/*      intr=^C         quit=^|         erase=del       kill=^U
        eof=^D          vtime=\0        vmin=\1         sxtc=\0
         start=^Q        stop=^S         susp=^Y         eol=\0
         reprint=^R      discard=^U      werase=^W       lnext=^V
         eol2=\0
*/

#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\031\0\022\017\027\026\0"




#endif /* TERMIOS_H_ */
