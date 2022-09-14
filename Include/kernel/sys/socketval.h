/*
 * socketval.h
 *
 *  Created on: Aug 14, 2013
 *      Author: root
 */

#ifndef SHAUN_SOCKETVAL_H_
#define SHAUN_SOCKETVAL_H_

#include <kernel/sys/protosw.h>
#include <kernel/sys/select.h>
#include <kernel/mbuf.h>
#include <kernel/sys/socket.h>
#include <kernel/io_dev.h>
#include <kernel/uio.h>
/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct socket {
	short	so_type;		/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	short	so_state;		/* internal state flags SS_*, below */
	caddr_t	so_pcb;			/* protocol control block */
	struct	protosw *so_proto;	/* protocol handle */
/*
 * Variables for connection queueing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket *so_head;	/* back pointer to accept socket */
	struct	socket *so_q0;		/* queue of partial connections */
	struct	socket *so_q;		/* queue of incoming connections */
	short	so_q0len;		/* partials on so_q0 */
	short	so_qlen;		/* number of connections on so_q */
	short	so_qlimit;		/* max number queued connections */
	short	so_timeo;		/* connection timeout */
	u16	so_error;		/* error affecting connection */
	pid_t	so_pgid;		/* pgid for signals */
	u32	so_oobmark;		/* chars to oob mark */
	wait_queue_head_t timoq;
/*
 * Variables for socket buffering.
 */
	struct	sockbuf {
		u32	sb_cc;		/* actual chars in buffer */
		u32	sb_hiwat;	/* max actual char count */
		u32	sb_mbcnt;	/* chars of mbufs used */
		u32	sb_mbmax;	/* max chars of mbufs to use */
		long	sb_lowat;	/* low water mark */
		struct	mbuf *sb_mb;	/* the mbuf chain */
		struct	selinfo sb_sel;	/* process selecting read/write */
		short	sb_flags;	/* flags, see below */
		short	sb_timeo;	/* timeout for read/write */
		wait_queue_head_t lockq;
		wait_queue_head_t waitq;
	} so_rcv, so_snd;
#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_NOTIFY	(SB_WAIT|SB_SEL|SB_ASYNC)
#define	SB_NOINTR	0x40		/* operations not interruptible */

	caddr_t	so_tpcb;		/* Wisc. protocol control block XXX */
	void	(*so_upcall)(struct socket *so, caddr_t arg, int waitf);
	caddr_t	so_upcallarg;		/* Arg for above */
};




/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x400	/* deciding to accept connection req */


static __inline int
imin(int a,int b)
{
	return (a < b ? a : b);
}
/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
#define	sbspace(sb) \
    ((long) imin((int)((sb)->sb_hiwat - (sb)->sb_cc), \
	 (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))





extern u32	sb_max;

#define	sorwakeup(so)	sowakeup((so), &(so)->so_rcv)

#define	sowwakeup(so)	sowakeup((so), &(so)->so_snd)




/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt += MCLBYTES; \
	}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt -= MCLBYTES; \
	}

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
#define sblock(sb, wf) ((sb)->sb_flags & SB_LOCK ? \
		(((wf) == M_WAITOK) ? sb_lock(sb) : -EWOULDBLOCK) : \
		((sb)->sb_flags |= SB_LOCK), 0)

/* release lock on sockbuf sb */
#define	sbunlock(sb) { \
	(sb)->sb_flags &= ~SB_LOCK; \
	if ((sb)->sb_flags & SB_WANT) { \
		(sb)->sb_flags &= ~SB_WANT; \
		sb_unlock(sb); \
	} \
}


/* to catch callers missing new second argument to sonewconn: */
#define	sonewconn(head, connstatus)	sonewconn1((head), (connstatus))
extern struct	socket *sonewconn1(struct socket *head, int connstatus);
extern void
soisdisconnected(struct socket *so);
extern void
soisconnected(struct socket *so);
extern int
sbappendaddr(struct sockbuf *sb, struct sockaddr *asa,
		struct mbuf *m0, struct mbuf *control);
extern void
sowakeup(struct socket *so, struct sockbuf *sb);
extern int
soreserve(struct socket *so, u32 sndcc, u32 rcvcc);
extern void
socantsendmore(struct socket *so);
extern int
sofree(struct socket *so);
extern int
sbreserve(struct sockbuf *sb, u32 cc);
extern void
sbdrop(struct sockbuf *sb, int len);
extern void
sbrelease(struct sockbuf *sb);
extern void
sbflush(struct sockbuf *sb);
extern void
sbdroprecord(struct sockbuf *sb);
extern void
socantrcvmore(struct socket *so);
extern void
sbappend(struct sockbuf *sb, struct mbuf *m);
extern void
sohasoutofband(struct socket *so);
extern int
soabort(struct socket *so);
extern void
soisconnecting(struct socket *so);
extern void
soisdisconnecting(struct socket *so);
extern int
sodisconnect(struct socket *so);
extern int
soqremque(struct socket *so, int q);
extern void
sorflush(struct socket *so);
extern int
sbsleep(wait_queue_head_t *wq, char *wchan, int ms);
extern int
sb_wakeup(wait_queue_head_t *wq);
extern int
sbwait(struct sockbuf *sb);
extern int
sb_lock(struct sockbuf *sb);
extern int
sb_unlock(struct sockbuf *sb);
extern int
soconnect(struct socket *so, struct mbuf *nam);
extern int
sosend(struct socket *so, struct mbuf *addr, struct uio *uio,
		struct mbuf *top, struct mbuf *control, int flags);
extern int
socreate(int dom, struct socket **aso, int type, int proto);
extern int
sobind(struct socket *so, struct mbuf *nam);
extern int
solisten(struct socket *so, int backlog);
extern int
soaccept(struct socket *so, struct mbuf *nam);
extern int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
		struct mbuf **mp0, struct mbuf **controlp, int *flagsp);

extern int
soshutdown(struct socket *so, int how);
extern int
soconnect2(struct socket *so1, struct socket *so2);
extern int
soclose(struct socket *so);
extern int
sogetopt(struct socket *so, int level, int optname, struct mbuf **mp);
extern int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m0);


#endif /* SHAUN_SOCKETVAL_H_ */

