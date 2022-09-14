/*
 * uipc_socket.c
 *
 *  Created on: Aug 15, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */


#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/sys/socketval.h>
#include <kernel/sys/domain.h>
#include <kernel/uio.h>
#include <kernel/mbuf.h>
#include <kernel/sys/protosw.h>
#include <string.h>
#include <kernel/timer.h>
#include <kernel/time.h>
#include <kernel/limits.h>

extern char	netcls[];

int
socreate(int dom, struct socket **aso, int type, int proto)
{
	struct protosw *prp;
	struct socket *so;
	int error;
	LOG_INFO("in");
	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);

	if (prp == NULL || prp->pr_usrreq == NULL)
		return -EPROTONOSUPPORT;

	if (prp->pr_type != type)
		return -EPROTOTYPE;

	so = (struct socket *)kzalloc(sizeof(*so));
	so->so_type = type;
	//TODO:permission check
	so->so_state |= SS_PRIV;

	list_init(&so->timoq.waiter);
	list_init(&so->so_rcv.lockq.waiter);
	list_init(&so->so_rcv.waitq.waiter);
	list_init(&so->so_snd.lockq.waiter);
	list_init(&so->so_snd.waitq.waiter);

	so->so_proto = prp;
	//to be fixed;
	so->so_options |= SO_BROADCAST;
	error = (*prp->pr_usrreq)(so, PRU_ATTACH, NULL, (struct mbuf *)proto, NULL);
	if (error) {
		so->so_state |= SS_NOFDREF;
		sofree(so);
		return error;
	}
	*aso = so;
	return 0;
}


int
sobind(struct socket *so, struct mbuf *nam)
{
	int error;
	error = (*so->so_proto->pr_usrreq)(so, PRU_BIND, NULL, nam, NULL);
	return error;
}

int
solisten(struct socket *so, int backlog)
{
	int error;
	error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN,
			NULL, NULL, NULL);
	if (error)
		return error;
	if (so->so_q == NULL)
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = min(backlog, SOMAXCONN);
	return 0;
}



int
sofree(struct socket *so)
{
	LOG_INFO("in");
	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
		return 0;
	if (so->so_head) {
		if (!soqremque(so, 0) && !soqremque(so, 1))
			LOG_INFO("sofree dq");
		so->so_head = 0;
	}
	sbrelease(&so->so_snd);
	sorflush(so);
	kfree(so);
	LOG_INFO("end");
	return 0;
}

void
sohasoutofband(struct socket *so)
{
//	struct proc *p;
//
//	if (so->so_pgid < 0)
//		gsignal(-so->so_pgid, SIGURG);
//	else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
//		psignal(p, SIGURG);
//	selwakeup(&so->so_rcv.sb_sel);
}



int
soabort(struct socket *so)
{
	return (
	    (*so->so_proto->pr_usrreq)(so, PRU_ABORT,
		(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0));
}



int
soclose(struct socket *so)
{
	int error = 0;
	LOG_INFO("in");
	if (so->so_options & SO_ACCEPTCONN) {
		while (so->so_q0)
			soabort(so->so_q0);
		while (so->so_q)
			soabort(so->so_q);
	}

	if (so->so_pcb == NULL)
		goto discard;

	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}

		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
					(so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				sbsleep(&so->timoq, netcls, so->so_linger * HZ);
			}
		}
	}

	drop:
	if(so->so_pcb) {
		int error2 = (*so->so_proto->pr_usrreq)(so, PRU_DETACH, NULL, NULL, NULL);
		if (error == 0)
			error = error2;
	}

	discard:
	if (so->so_state & SS_NOFDREF)
		LOG_INFO("no fd ref");
	so->so_state |= SS_NOFDREF;
	sofree(so);
	return error;
}

int
soaccept(struct socket *so, struct mbuf *nam)
{
	int error;

	if ((so->so_state & SS_NOFDREF))
		LOG_INFO("NOFDREF");

	so->so_state &= ~ SS_NOFDREF;

	error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT, NULL, nam, NULL);

	return error;
}


int
soconnect(struct socket *so, struct mbuf *nam)
{
	int error;
	if (so->so_options & SO_ACCEPTCONN)
		return 	-EOPNOTSUPP;

	if ((so->so_state & (SS_ISCONNECTED | SS_ISCONNECTING)) &&
			((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
					(error = sodisconnect(so))))
		error = -EISCONN;
	else
		error = (*so->so_proto->pr_usrreq)(so, PRU_CONNECT, NULL, nam, NULL);

	return error;
}


int
soconnect2(struct socket *so1, struct socket *so2)
{
	int error;

	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2,
	    NULL, (struct mbuf *)so2, NULL);

	return error;
}

int
sodisconnect(struct socket *so)
{
	int error;

	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = -ENOTCONN;
		goto end;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = -EALREADY;
		goto end;
	}
	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT,
	    NULL, NULL, NULL);
end:
	return error;
}

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)

int
sosend(struct socket *so, struct mbuf *addr, struct uio *uio,
		struct mbuf *top, struct mbuf *control, int flags)
{
	struct mbuf *m, **mp;
	u32	space, len, resid;
	int clen = 0;
	int error, dontroute, mlen;
	int atomic = sosendallatonce(so) || top;

	LOG_INFO("in");
	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;

	if (resid < 0)
		return -EINVAL;

	dontroute = (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
			(so->so_proto->pr_flags & PR_ATOMIC);

	if (control)
		clen = control->m_len;

#define senderr(errno)	{ error = errno; goto release;}

	restart:
	if ((error = sblock(&so->so_snd, SBLOCKWAIT(flags))))
		goto out;

	do {
		if (so->so_state & SS_CANTSENDMORE)
			senderr(-EPIPE);
		if (so->so_error)
			senderr(so->so_error);

		if ((so->so_state & SS_ISCONNECTED) ==0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
						!(resid == 0 && clen != 0))
					senderr(-ENOTCONN);
			} else if (addr == 0)
				senderr(-EDESTADDRREQ);
		}

		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;

		if ((atomic && resid > so->so_snd.sb_hiwat) ||
				clen > so->so_snd.sb_hiwat)
			senderr(-EMSGSIZE);

		if (space < resid + clen && uio && (atomic ||
				space < so->so_snd.sb_lowat || space < clen)) {
			if (so->so_state & SS_NBIO)
				senderr(-EWOULDBLOCK);
			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);
			if (error)
				goto out;
			goto restart;
		}
		mp = &top;
		space -= clen;

		do {
			if (uio == NULL) {
				resid = 0;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			} else do {
				if (top == NULL) {
					MGETHDR(m, M_WAIT, MT_DATA);
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = NULL;
				} else {
					MGET(m, M_WAIT, MT_DATA);
					mlen = MLEN;
				}

				if (resid >= MINCLSIZE && space >= MCLBYTES) {
					MCLGET(m, M_WAIT);
					if ((m->m_flags & M_EXT) == 0)
						goto nopages;
					mlen = MCLBYTES;

					if (atomic && top == NULL) {
						len = min(MCLBYTES - max_hdr, resid);
						m->m_data += max_hdr;
					} else
						len = min(MCLBYTES, resid);

					space -= MCLBYTES;
				} else {
					nopages:
					len = min(min(mlen, resid), space);
					space -= len;

					if (atomic && top == NULL && len < mlen)
						MH_ALIGN(m, len);
				}

				error = uiomove(mtod(m, caddr_t), len, uio);
				resid = uio->uio_resid;
				m->m_len = len;
				*mp = m;
				top->m_pkthdr.len += len;
				if (error)
					goto release;

				mp = &m->m_next;
				if(resid <= 0) {
					if (flags & MSG_EOR)
						top->m_flags |= M_EOR;
					break;
				}
			} while (space > 0 && atomic);


			if (dontroute)
				so->so_options |= SO_DONTROUTE;

			error = (*so->so_proto->pr_usrreq)(so,
					(flags & MSG_OOB)?PRU_SENDOOB:PRU_SEND,
							top, addr, control);
			if (dontroute)
				so->so_options &= ~ SO_DONTROUTE;
			clen = 0;
			control = 0;
			top = 0;
			mp = &top;
			if (error)
				goto release;

		} while (resid && space > 0);

	} while (resid);

	release:
	sbunlock(&so->so_snd);
	out:
	LOG_INFO("out.");
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return error;
}

int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
		struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct mbuf *m, **mp;
	int flags, len, error, offset;
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	int moff, type;
	int orig_resid = uio->uio_resid;

	mp = mp0;
	if (paddr)
		*paddr = NULL;
	if (controlp)
		*controlp = NULL;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m,
				(struct mbuf *)(long)(flags & MSG_PEEK), NULL);
		if (error)
			goto bad;

		do {
			error = uiomove(mtod(m, caddr_t), (int)min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);

		bad:
		if (m)
			m_freem(m);
		return error;
	}

	if (mp)
		*mp = NULL;
	if ((so->so_state & SS_ISCONFIRMING) && uio->uio_resid)
		(*pr->pr_usrreq)(so, PRU_RCVD, NULL, NULL, NULL);

	restart:
	if ((error = sblock(&so->so_rcv, SBLOCKWAIT(flags))))
		return error;
	m = so->so_rcv.sb_mb;

	if ((m == NULL) || (((flags & MSG_DONTWAIT) == 0 && so->so_rcv.sb_cc < uio->uio_resid) &&
			(so->so_rcv.sb_cc < so->so_rcv.sb_lowat || ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
			m->m_nextpkt == NULL && (pr->pr_flags & PR_ATOMIC) == 0)) {
		if (so->so_error) {
			if (m)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}

		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				goto dontblock;
			else
				goto release;
		}

		for (; m; m = m->m_next	)
			if (m->m_type == MT_OOBDATA || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}

		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
				(so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = -ENOTCONN;
			goto release;
		}

		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = -EWOULDBLOCK;
			goto release;
		}
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		if (error)
			return error;
		goto restart;
	}
	dontblock:
	nextrecord = m->m_nextpkt;

	if (pr->pr_flags & PR_ADDR) {
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
	}

	while (m && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (controlp) {
				if (pr->pr_domain->dom_externalize &&
						mtod(m, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
					error = (*pr->pr_domain->dom_externalize)(m);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}

		if (controlp) {
			orig_resid = 0;
			controlp = & (*controlp)->m_next;
		}
	}

	if (m) {
		if ((flags & MSG_PEEK) == 0)
			m->m_nextpkt = nextrecord;
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	}

	moff = 0;
	offset = 0;

	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;

		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		if (mp == NULL) {
			error = uiomove(mtod(m, caddr_t) + moff, len, uio);
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = NULL;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
					m = so->so_rcv.sb_mb;
				}
				if (m)
					m->m_nextpkt = nextrecord;
			}

		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}

		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}

		if (flags & MSG_EOR)
			break;

		while ((flags & MSG_WAITALL) && m == NULL && uio->uio_resid > 0 &&
				!sosendallatonce(so) && !nextrecord	) {
			if (so->so_error || (so->so_state & SS_CANTRCVMORE))
				break;

			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				return 0;
			}
			if (!(m = so->so_rcv.sb_mb))
				nextrecord = m->m_nextpkt;
		}

	}

	if (m && (pr->pr_flags & PR_ATOMIC)) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			sbdroprecord(&so->so_rcv);
	}

	if ((flags & MSG_PEEK) == 0) {
		if (m == NULL)
			so->so_rcv.sb_mb = nextrecord;
		if ((pr->pr_flags & PR_WANTRCVD) && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, NULL, (struct mbuf *)(long)flags, NULL, NULL);
	}

	if (orig_resid == uio->uio_resid && orig_resid &&
			(flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		goto restart;
	}
	if (flagsp)
		*flagsp |= flags;
	release:
	sbunlock(&so->so_rcv);
	return error;
}


int
soshutdown(struct socket *so, int how)
{
	struct protosw *pr = so->so_proto;

	how++;
	if (how & FREAD)
		sorflush(so);
	if (how & FWRITE)
		return ((*pr->pr_usrreq)(so, PRU_SHUTDOWN,
		    NULL, NULL, NULL));
	return (0);
}


void
sorflush(struct socket *so)
{
	struct sockbuf *sb = &so->so_rcv;
	struct protosw *pr = so->so_proto;

	struct sockbuf asb;

	LOG_INFO("in");
	sb->sb_flags |= SB_NOINTR;
	(void) sblock(sb, M_WAITOK);

	socantrcvmore(so);
	sbunlock(sb);
	asb = *sb;
	bzero((caddr_t)sb, sizeof (*sb));

	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
	sbrelease(&asb);
	LOG_INFO("end");
}

int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m0)
{
	int error = 0;
	register struct mbuf *m = m0;

	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput)
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
		error = ENOPROTOOPT;
	} else {
		switch (optname) {

		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof (struct linger)) {
				error = -EINVAL;
				goto bad;
			}
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* fall thru... */

		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_OOBINLINE:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = -EINVAL;
				goto bad;
			}
			switch (optname) {

			case SO_SNDBUF:
			case SO_RCVBUF:
				if (sbreserve(optname == SO_SNDBUF ?
				    &so->so_snd : &so->so_rcv,
				    (u32) *mtod(m, int *)) == 0) {
					error = -ENOBUFS;
					goto bad;
				}
				break;

			case SO_SNDLOWAT:
				so->so_snd.sb_lowat = *mtod(m, int *);
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat = *mtod(m, int *);
				break;
			}
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			struct timeval *tv;
			short val;
			int	tick = 1000000 / HZ;

			if (m == NULL || m->m_len < sizeof (*tv)) {
				error = -EINVAL;
				goto bad;
			}
			tv = mtod(m, struct timeval *);
			if (tv->tv_sec * HZ + tv->tv_usec / (u32)tick > SHRT_MAX) {
				error = -EDOM;
				goto bad;
			}
			val = tv->tv_sec * HZ + tv->tv_usec / tick;

			switch (optname) {

			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = val;
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = val;
				break;
			}
			break;
		    }

		default:
			error = -ENOPROTOOPT;
			break;
		}
		if (error == 0 && so->so_proto && so->so_proto->pr_ctloutput) {
			(void) ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
			m = NULL;	/* freed by protocol */
		}
	}
bad:
	if (m)
		(void) m_free(m);
	return (error);
}

int
sogetopt(struct socket *so, int level, int optname, struct mbuf **mp)
{
	register struct mbuf *m;
	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_GETOPT, so, level, optname, mp));
		} else
			return (-ENOPROTOOPT);
	} else {
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof (int);

		switch (optname) {

		case SO_LINGER:
			m->m_len = sizeof (struct linger);
			mtod(m, struct linger *)->l_onoff =
				so->so_options & SO_LINGER;
			mtod(m, struct linger *)->l_linger = so->so_linger;
			break;

		case SO_USELOOPBACK:
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_BROADCAST:
		case SO_OOBINLINE:
			*mtod(m, int *) = so->so_options & optname;
			break;

		case SO_TYPE:
			*mtod(m, int *) = so->so_type;
			break;

		case SO_ERROR:
			*mtod(m, int *) = so->so_error;
			so->so_error = 0;
			break;

		case SO_SNDBUF:
			*mtod(m, int *) = so->so_snd.sb_hiwat;
			break;

		case SO_RCVBUF:
			*mtod(m, int *) = so->so_rcv.sb_hiwat;
			break;

		case SO_SNDLOWAT:
			*mtod(m, int *) = so->so_snd.sb_lowat;
			break;

		case SO_RCVLOWAT:
			*mtod(m, int *) = so->so_rcv.sb_lowat;
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
		    int	tick = 1000000 / HZ;

			int val = (optname == SO_SNDTIMEO ?
			     so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			m->m_len = sizeof(struct timeval);
			mtod(m, struct timeval *)->tv_sec = val / HZ;
			mtod(m, struct timeval *)->tv_usec =
			    (val % HZ) * tick;
			break;
		    }

		default:
			(void)m_free(m);
			return (-ENOPROTOOPT);
		}
		*mp = m;
		return (0);
	}
}



