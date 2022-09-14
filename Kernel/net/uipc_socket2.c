/*
 * uipc_socket2.c
 *
 *  Created on: Aug 15, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/sys/socketval.h>
#include <kernel/mbuf.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/kthread.h>
#include <kernel/timer.h>

#define MALLOC(p, t, s) p = (t)kmalloc((s))
#define FREE(p)	kfree((p));

u32	sb_max = SB_MAX;

/* strings for sleep message: */
char	netio[] = "netio";
char	netcon[] = "netcon";
char	netcls[] = "netcls";

void
soisconnecting(struct socket *so)
{

	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
}

void
soisdisconnecting(struct socket *so)
{

	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);
	sb_wakeup(&so->timoq);
	sowwakeup(so);
	sorwakeup(so);
}

void
soqinsque(struct socket *head, struct socket *so, int q)
{

	struct socket **prev;
	so->so_head = head;
	if (q == 0) {
		head->so_q0len++;
		so->so_q0 = 0;
		for (prev = &(head->so_q0); *prev; )
			prev = &((*prev)->so_q0);
	} else {
		head->so_qlen++;
		so->so_q = 0;
		for (prev = &(head->so_q); *prev; )
			prev = &((*prev)->so_q);
	}
	*prev = so;
}

int
soqremque(struct socket *so, int q)
{
	struct socket *head, *prev, *next;

	head = so->so_head;
	prev = head;
	for (;;) {
		next = q ? prev->so_q : prev->so_q0;
		if (next == so)
			break;
		if (next == 0)
			return (0);
		prev = next;
	}
	if (q == 0) {
		prev->so_q0 = next->so_q0;
		head->so_q0len--;
	} else {
		prev->so_q = next->so_q;
		head->so_qlen--;
	}
	next->so_q0 = next->so_q = 0;
	next->so_head = 0;
	return (1);
}

void
soisconnected(struct socket *so)
{
	struct socket *head = so->so_head;

	so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING|SS_ISCONFIRMING);
	so->so_state |= SS_ISCONNECTED;
	if (head && soqremque(so, 0)) {
		soqinsque(head, so, 1);
		sorwakeup(head);
		sb_wakeup(&head->timoq);
	} else {
		sb_wakeup(&so->timoq);
		sorwakeup(so);
		sowwakeup(so);
	}
}

void
soisdisconnected(struct socket *so)
{
	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);
	sb_wakeup(&so->timoq);
	sowwakeup(so);
	sorwakeup(so);
}



/*
 * Append address and data, and optionally, control (ancillary) data
 * to the receive queue of a socket.  If present,
 * m0 must include a packet header with total length.
 * Returns 0 if no space in sockbuf or insufficient mbufs.
 */
int
sbappendaddr(struct sockbuf *sb, struct sockaddr *asa,
		struct mbuf *m0, struct mbuf *control)
{
	struct mbuf *m, *n;
	int space = asa->sa_len;

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
			panic("sbappendaddr");
	if (m0)
		space += m0->m_pkthdr.len;
	for (n = control; n; n = n->m_next) {
		space += n->m_len;
		if (n->m_next == 0)	/* keep pointer to last control buf */
			break;
	}
	if (space > sbspace(sb))
		return (0);
	if (asa->sa_len > MLEN)
		return (0);
	MGET(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);
	m->m_len = asa->sa_len;
	bcopy((caddr_t)asa, mtod(m, caddr_t), asa->sa_len);
	if (n)
		n->m_next = m0;		/* concatenate data to control */
	else
		control = m0;
	m->m_next = control;
	for (n = m; n; n = n->m_next)
		sballoc(sb, n);
	if ((n = sb->sb_mb) != NULL) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		n->m_nextpkt = m;
	} else
		sb->sb_mb = m;
	return (1);
}
/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup(struct socket *so, struct sockbuf *sb)
{

	//selwakeup(&sb->sb_sel);
	sb->sb_flags &= ~SB_SEL;
	if (sb->sb_flags & SB_WAIT) {
		sb->sb_flags &= ~SB_WAIT;
		sb_wakeup(&sb->waitq);
	}
//	if (so->so_state & SS_ASYNC) {
//		if (so->so_pgid < 0)
//			gsignal(-so->so_pgid, SIGIO);
//		else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
//			psignal(p, SIGIO);
//	}
}




/*
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 */

void
socantsendmore(struct socket *so)
{

	so->so_state |= SS_CANTSENDMORE;
	sowwakeup(so);
}


int
sbreserve(struct sockbuf *sb, u32 cc)
{

	if (cc > sb_max * MCLBYTES / (MSIZE + MCLBYTES))
		return (0);
	sb->sb_hiwat = cc;
	sb->sb_mbmax = min(cc * 2, sb_max);
	if (sb->sb_lowat > sb->sb_hiwat)
		sb->sb_lowat = sb->sb_hiwat;
	return (1);
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop(struct sockbuf *sb, int len)
{
	struct mbuf *m, *mn;
	struct mbuf *next;

	next = (m = sb->sb_mb) ? m->m_nextpkt : 0;
	while (len > 0) {
		if (m == 0) {
			if (next == 0)
				panic("sbdrop");
			m = next;
			next = m->m_nextpkt;
			continue;
		}
		if (m->m_len > len) {
			m->m_len -= len;
			m->m_data += len;
			sb->sb_cc -= len;
			break;
		}
		len -= m->m_len;
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}
	while (m && m->m_len == 0) {
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}
	if (m) {
		sb->sb_mb = m;
		m->m_nextpkt = next;
	} else
		sb->sb_mb = next;
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush(struct sockbuf *sb)
{
	if (sb->sb_flags & SB_LOCK)
		panic("sbflush");
	while (sb->sb_mbcnt)
		sbdrop(sb, (int)sb->sb_cc);
	if (sb->sb_cc || sb->sb_mb)
		panic("sbflush 2");
}


void
sbrelease(struct sockbuf *sb)
{
	LOG_INFO("in");
	sbflush(sb);
	sb->sb_hiwat = sb->sb_mbmax = 0;
	LOG_INFO("end");
}

int
soreserve(struct socket *so, u32 sndcc, u32 rcvcc)
{

	if (sbreserve(&so->so_snd, sndcc) == 0)
		goto bad;
	if (sbreserve(&so->so_rcv, rcvcc) == 0)
		goto bad2;
	if (so->so_rcv.sb_lowat == 0)
		so->so_rcv.sb_lowat = 1;
	if (so->so_snd.sb_lowat == 0)
		so->so_snd.sb_lowat = MCLBYTES;
	if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat;
	return (0);
bad2:
	sbrelease(&so->so_snd);
bad:
	return (-ENOBUFS);
}

/*
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 */
void
sbdroprecord(struct sockbuf *sb)
{
	struct mbuf *m, *mn;
	m = sb->sb_mb;
	if (m) {
		sb->sb_mb = m->m_nextpkt;
		do {
			sbfree(sb, m);
			MFREE(m, mn);
		} while ((m = mn) != NULL);
	}
}


/*
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 */
void
sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n)
{
	int eor = 0;
	struct mbuf *o;

	while (m) {
		eor |= m->m_flags & M_EOR;
		if (m->m_len == 0 &&
		    (eor == 0 ||
		     (((o = m->m_next) || (o = n)) &&
		      o->m_type == m->m_type))) {
			m = m_free(m);
			continue;
		}
		if (n && (n->m_flags & (M_EXT | M_EOR)) == 0 &&
		    (n->m_data + n->m_len + m->m_len) < &n->m_dat[MLEN] &&
		    n->m_type == m->m_type) {
			bcopy(mtod(m, caddr_t), mtod(n, caddr_t) + n->m_len,
			    (unsigned)m->m_len);
			n->m_len += m->m_len;
			sb->sb_cc += m->m_len;
			m = m_free(m);
			continue;
		}
		if (n)
			n->m_next = m;
		else
			sb->sb_mb = m;
		sballoc(sb, m);
		n = m;
		m->m_flags &= ~M_EOR;
		m = m->m_next;
		n->m_next = 0;
	}
	if (eor) {
		if (n)
			n->m_flags |= eor;
		else
			LOG_INFO("semi-panic: sbcompress");
	}
}

/*
 * As above, except the mbuf chain
 * begins a new record.
 */
void
sbappendrecord(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf *m;

	if (m0 == 0)
		return;
	if ((m = sb->sb_mb) != NULL)
		while (m->m_nextpkt)
			m = m->m_nextpkt;
	/*
	 * Put the first mbuf on the queue.
	 * Note this permits zero length records.
	 */
	sballoc(sb, m0);
	if (m)
		m->m_nextpkt = m0;
	else
		sb->sb_mb = m0;
	m = m0->m_next;
	m0->m_next = 0;
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	sbcompress(sb, m, m0);
}

void
sbappend(struct sockbuf *sb, struct mbuf *m)
{
	struct mbuf *n;

	if (m == 0)
		return;
	if ((n = sb->sb_mb) != NULL) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		do {
			if (n->m_flags & M_EOR) {
				sbappendrecord(sb, m); /* XXXXXX!!!! */
				return;
			}
		} while (n->m_next && (n = n->m_next));
	}
	sbcompress(sb, m, n);
}

void
socantrcvmore(struct socket *so)
{

	so->so_state |= SS_CANTRCVMORE;
	sorwakeup(so);
}



/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, propoerly linked into the
 * data structure of the original socket, and return this.
 * Connstatus may be 0, or SO_ISCONFIRMING, or SO_ISCONNECTED.
 *
 * Currently, sonewconn() is defined as sonewconn1() in socketvar.h
 * to catch calls that are missing the (new) second parameter.
 */
struct socket *
sonewconn1(struct socket *head, int connstatus)
{
	struct socket *so;
	int soqueue = connstatus ? 1 : 0;

	if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
		return ((struct socket *)0);
	MALLOC(so, struct socket *, sizeof(*so));
	if (so == NULL)
		return ((struct socket *)0);
	bzero((caddr_t)so, sizeof(*so));
	so->so_type = head->so_type;
	so->so_options = head->so_options &~ SO_ACCEPTCONN;
	so->so_linger = head->so_linger;
	so->so_state = head->so_state | SS_NOFDREF;
	so->so_proto = head->so_proto;
	so->so_timeo = head->so_timeo;
	so->so_pgid = head->so_pgid;

	list_init(&so->timoq.waiter);
	list_init(&so->so_rcv.lockq.waiter);
	list_init(&so->so_rcv.waitq.waiter);
	list_init(&so->so_snd.lockq.waiter);
	list_init(&so->so_snd.waitq.waiter);

	(void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
	soqinsque(head, so, soqueue);
	if ((*so->so_proto->pr_usrreq)(so, PRU_ATTACH,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0)) {
		(void) soqremque(so, soqueue);
		(void) kfree((caddr_t)so);
		return ((struct socket *)0);
	}
	if (connstatus) {
		sorwakeup(head);
		sb_wakeup(&head->timoq);
		so->so_state |= connstatus;
	}
	return (so);
}


/*
 * Lock a sockbuf already known to be locked;
 * return any error returned from sleep (EINTR).
 */
int
sb_lock(struct sockbuf *sb)
{
	int error;

	while (sb->sb_flags & SB_LOCK) {
		sb->sb_flags |= SB_WANT;
		if ((error = sbsleep(&sb->lockq, netio, 0)))
			return (error);
	}
	sb->sb_flags |= SB_LOCK;
	return (0);
}


/*
 * Wait for data to arrive at/drain from a socket buffer.
 */
int
sbwait(struct sockbuf *sb)
{
	sb->sb_flags |= SB_WAIT;
	return (sbsleep(&sb->waitq, netio, sb->sb_timeo));
}

int
sbsleep(wait_queue_head_t *wq, char *wchan, int ms)
{
	int timer_id;
	int ret;

	if (ms < 0)
		return -EINVAL;
	if (ms == 0) {
		//sleep forever
		pt_wait_on(wq);
		return 0;
	}

	ret = register_timer(ms, TIMER_FLAG_ONESHUT | TIMER_FLAG_ENABLED,
			pt_timer_callback, (u32)current);
	if (ret < 0)
		return ret;
	timer_id = ret;

	pt_wait_on(wq);
	unregister_timer(timer_id);
	return 0;
}

int
sb_wakeup(wait_queue_head_t *wq)
{
	struct linked_list *p, *n;
	kthread_t *t;

	for (p = wq->waiter.next, n = p->next;
			p != &wq->waiter; p = n, n = n->next) {
		t = container_of(p, kthread_t, thread_list);
		pt_wakeup(t);
	}
	return 0;
}


int sb_unlock(struct sockbuf *sb)
{
	sb_wakeup(&sb->lockq);
	return 0;
}
