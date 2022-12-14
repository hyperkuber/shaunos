/*
 * raw_ip.c
 *
 *  Created on: Aug 1, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/types.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/socketval.h>
#include <kernel/mbuf.h>
#include <kernel/kernel.h>

struct inpcb rawinpcb;

extern u16 ip_id;
extern struct ifnet *ifnet;
/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

/*
 * Initialize raw connection block q.
 */
void
rip_init()
{

	rawinpcb.inp_next = rawinpcb.inp_prev = &rawinpcb;
}

struct	sockaddr_in ripsrc = { sizeof(ripsrc), AF_INET };
/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct inpcb *inp;
	struct socket *last = 0;
	LOG_INFO("in");
	ripsrc.sin_addr = ip->ip_src;
	for (inp = rawinpcb.inp_next; inp != &rawinpcb; inp = inp->inp_next) {
		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != ip->ip_p)
			continue;
		if (inp->inp_laddr.s_addr &&
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
			continue;
		if (inp->inp_faddr.s_addr &&
		    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
			continue;
		if (last) {
			struct mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (sbappendaddr(&last->so_rcv,
				    (struct sockaddr *)&ripsrc, n,
				    (struct mbuf *)0) == 0)
					/* should notify about lost packet */
					m_freem(n);
				else
					sorwakeup(last);
			}
		}
		last = inp->inp_socket;
	}
	if (last) {
		if (sbappendaddr(&last->so_rcv, (struct sockaddr *)&ripsrc,
		    m, (struct mbuf *)0) == 0)
			m_freem(m);
		else
			sorwakeup(last);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}

	LOG_INFO("end");
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(struct mbuf *m, struct socket *so, u32 dst)
{
	struct ip *ip;
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *opts;
	int flags = (so->so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		M_PREPEND(m, sizeof(struct ip), M_WAIT);
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_off = 0;
		ip->ip_p = inp->inp_ip.ip_p;
		ip->ip_len = m->m_pkthdr.len;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = dst;
		ip->ip_ttl = MAXTTL;
		opts = inp->inp_options;
	} else {
		ip = mtod(m, struct ip *);
		if (ip->ip_id == 0) {
			ip->ip_id = htons(ip_id);
			ip_id++;
		}

		opts = NULL;
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		ipstat.ips_rawout++;
	}
	return (ip_output(m, opts, &inp->inp_route, flags, inp->inp_moptions));
}

/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(int op, struct socket *so, int level, int optname, struct mbuf **m)
{
	struct inpcb *inp = sotoinpcb(so);
	int error;

	if (level != IPPROTO_IP) {
		if (op == PRCO_SETOPT && *m)
			(void) m_free(*m);
		return (-EINVAL);
	}

	switch (optname) {

	case IP_HDRINCL:
		error = 0;
		if (op == PRCO_SETOPT) {
			if (*m == 0 || (*m)->m_len < sizeof (int))
				error = -EINVAL;
			else if (*mtod(*m, int *))
				inp->inp_flags |= INP_HDRINCL;
			else
				inp->inp_flags &= ~INP_HDRINCL;
			if (*m)
				(void)m_free(*m);
		} else {
			*m = m_get(M_WAIT, MT_SOOPTS);
			(*m)->m_len = sizeof (int);
			*mtod(*m, int *) = inp->inp_flags & INP_HDRINCL;
		}
		return (error);

	case DVMRP_INIT:
	case DVMRP_DONE:
	case DVMRP_ADD_VIF:
	case DVMRP_DEL_VIF:
	case DVMRP_ADD_LGRP:
	case DVMRP_DEL_LGRP:
	case DVMRP_ADD_MRT:
	case DVMRP_DEL_MRT:
#ifdef MROUTING
		if (op == PRCO_SETOPT) {
			error = ip_mrouter_cmd(optname, so, *m);
			if (*m)
				(void)m_free(*m);
		} else
			error = EINVAL;
		return (error);
#else
		if (op == PRCO_SETOPT && *m)
			(void)m_free(*m);
		return (-EOPNOTSUPP);
#endif

	default:
		if (optname >= DVMRP_INIT) {
#ifdef MROUTING
			if (op == PRCO_SETOPT) {
				error = ip_mrouter_cmd(optname, so, *m);
				if (*m)
					(void)m_free(*m);
			} else
				error = EINVAL;
			return (error);
#else
			if (op == PRCO_SETOPT && *m)
				(void)m_free(*m);
			return (-EOPNOTSUPP);
#endif
		}

	}
	return (ip_ctloutput(op, so, level, optname, m));
}

u32	rip_sendspace = RIPSNDQ;
u32	rip_recvspace = RIPRCVQ;

int
rip_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control)
{
	int error = 0;
	struct inpcb *inp = sotoinpcb(so);
#ifdef MROUTING
	extern struct socket *ip_mrouter;
#endif
	switch (req) {

	case PRU_ATTACH:
		if (inp)
			panic("rip_attach");
		if ((so->so_state & SS_PRIV) == 0) {
			error = -EACCES;
			break;
		}
		if ((error = soreserve(so, rip_sendspace, rip_recvspace)) ||
		    (error = in_pcballoc(so, &rawinpcb)))
			break;
		inp = (struct inpcb *)so->so_pcb;
		inp->inp_ip.ip_p = (int)nam;
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = -ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (inp == 0)
			panic("rip_detach");
#ifdef MROUTING
		if (so == ip_mrouter)
			ip_mrouter_done();
#endif
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = -EINVAL;
			break;
		}
		if ((ifnet == 0) ||
		    ((addr->sin_family != AF_INET) &&
		     (addr->sin_family != AF_IMPLINK)) ||
		    (addr->sin_addr.s_addr &&
		     ifa_ifwithaddr((struct sockaddr *)addr) == 0)) {
			error = -EADDRNOTAVAIL;
			break;
		}
		inp->inp_laddr = addr->sin_addr;
		break;
	    }
	case PRU_CONNECT:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = -EINVAL;
			break;
		}
		if (ifnet == 0) {
			error = -EADDRNOTAVAIL;
			break;
		}
		if ((addr->sin_family != AF_INET) &&
		     (addr->sin_family != AF_IMPLINK)) {
			error = -EAFNOSUPPORT;
			break;
		}
		inp->inp_faddr = addr->sin_addr;
		soisconnected(so);
		break;
	    }

	case PRU_CONNECT2:
		error = -EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
	    {

			u32 dst;

			if (so->so_state & SS_ISCONNECTED) {
				if (nam) {
					error = -EISCONN;
					break;
				}
				dst = inp->inp_faddr.s_addr;
			} else {
				if (nam == NULL) {
					error = -ENOTCONN;
					break;
				}
				dst = mtod(nam, struct sockaddr_in *)->sin_addr.s_addr;
			}
				error = rip_output(m, so, dst);
				m = NULL;
				break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = -EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	default:
		panic("rip_usrreq");
	}
	if (m != NULL)
		m_freem(m);
	return (error);
}
