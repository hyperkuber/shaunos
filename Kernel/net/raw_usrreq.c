/*
 * raw_usrreq.c
 *
 *  Created on: Aug 14, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <kernel/net/raw_cb.h>
#include <kernel/sys/socketval.h>
#include <kernel/sys/socket.h>
#include <kernel/net/route.h>
#include <string.h>



void
raw_init()
{
	rawcb.rcb_next = rawcb.rcb_prev = &rawcb;
}

void
raw_input(struct mbuf *m0, struct sockproto *proto,
		struct sockaddr *src, struct sockaddr *dst)
{
	struct rawcb *rp;
	struct mbuf *m = m0;
	int sockets = 0;
	struct socket *last;

	LOG_INFO("in");
	last = 0;
	for (rp = rawcb.rcb_next; rp != &rawcb; rp = rp->rcb_next) {
		if (rp->rcb_proto.sp_family != proto->sp_family)
			continue;
		if (rp->rcb_proto.sp_protocol  &&
		    rp->rcb_proto.sp_protocol != proto->sp_protocol)
			continue;
		/*
		 * We assume the lower level routines have
		 * placed the address in a canonical format
		 * suitable for a structure comparison.
		 *
		 * Note that if the lengths are not the same
		 * the comparison will fail at the first byte.
		 */

		if (rp->rcb_laddr && !equal(rp->rcb_laddr, dst))
			continue;
		if (rp->rcb_faddr && !equal(rp->rcb_faddr, src))
			continue;
		if (last) {
			struct mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (sbappendaddr(&last->so_rcv, src,
				    n, (struct mbuf *)0) == 0)
					/* should notify about lost packet */
					m_freem(n);
				else {
					sorwakeup(last);
					sockets++;
				}
			}
		}
		last = rp->rcb_socket;
	}
	if (last) {
		if (sbappendaddr(&last->so_rcv, src,
		    m, (struct mbuf *)0) == 0)
			m_freem(m);
		else {
			sorwakeup(last);
			sockets++;
		}
	} else
		m_freem(m);
}


void
raw_ctlinput(int cmd, struct sockaddr *arg)
{

	if (cmd < 0 || cmd > PRC_NCMDS)
		return;
	/* INCOMPLETE */
}


int
raw_usrreq(struct socket *so, int req, struct mbuf *m,
		struct mbuf *nam, struct mbuf *control)
{
	struct rawcb *rp = sotorawcb(so);
	int error = 0;
	int len;

	if (req == PRU_CONTROL)
		return (-EOPNOTSUPP);
	if (control && control->m_len) {
		error = -EOPNOTSUPP;
		goto release;
	}
	if (rp == 0) {
		error = -EINVAL;
		goto release;
	}
	switch (req) {

	/*
	 * Allocate a raw control block and fill in the
	 * necessary info to allow packets to be routed to
	 * the appropriate raw interface routine.
	 */
	case PRU_ATTACH:	//for socket syscall
		if ((so->so_state & SS_PRIV) == 0) {
			error = -EACCES;
			break;
		}
		error = raw_attach(so, (int)nam);
		break;

	/*
	 * Destroy state just before socket deallocation.
	 * Flush data or not depending on the options.
	 */
	case PRU_DETACH:	//for close syscall
		if (rp == 0) {
			error = -ENOTCONN;
			break;
		}
		raw_detach(rp);
		break;

	case PRU_CONNECT2:	//for socketpair syscall
		error = -EOPNOTSUPP;
		goto release;

	case PRU_DISCONNECT:
		if (rp->rcb_faddr == 0) {
			error = -ENOTCONN;
			break;
		}
		raw_disconnect(rp);
		soisdisconnected(so);
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:	//shutdown syscall
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
		if (nam) {	//for sendto or sendmsg
			if (rp->rcb_faddr) {	//we can not send data with dest address vir route socket
				error = -EISCONN;
				break;
			}
			rp->rcb_faddr = mtod(nam, struct sockaddr *);	//get dest address
		} else if (rp->rcb_faddr == 0) {
			error = -ENOTCONN;
			break;
		}
		error = (*so->so_proto->pr_output)(m, so);	//for route socket, pr_output is route_output
		m = NULL;
		if (nam)
			rp->rcb_faddr = 0;
		break;

	case PRU_ABORT:
		raw_disconnect(rp);
		sofree(so);
		soisdisconnected(so);
		break;

	case PRU_SENSE:	//fstat syscall
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
		return(-EOPNOTSUPP);

	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = -EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:	//getsockname syscall
		if (rp->rcb_laddr == 0) {
			error = -EINVAL;
			break;
		}
		len = rp->rcb_laddr->sa_len;
		Bcopy((caddr_t)rp->rcb_laddr, mtod(nam, caddr_t), (unsigned)len);
		nam->m_len = len;
		break;

	case PRU_PEERADDR:	//getpeername syscall
		if (rp->rcb_faddr == 0) {
			error = -ENOTCONN;
			break;
		}
		len = rp->rcb_faddr->sa_len;
		Bcopy((caddr_t)rp->rcb_faddr, mtod(nam, caddr_t), (unsigned)len);
		nam->m_len = len;
		break;

	default:
		LOG_ERROR("raw_usrreq");
	}
release:
	if (m != NULL)
		m_freem(m);
	return (error);
}











