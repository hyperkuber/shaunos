/*
 * raw_cb.c
 *
 *  Created on: Aug 14, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/net/raw_cb.h>
#include <kernel/malloc.h>


u32	raw_sendspace = RAWSNDQ;
u32	raw_recvspace = RAWRCVQ;


/*
 * Allocate a control block and a nominal amount
 * of buffer space for the socket.
 */
int
raw_attach(struct socket *so, int proto)
{
	struct rawcb *rp = sotorawcb(so);
	int error;

	/*
	 * It is assumed that raw_attach is called
	 * after space has been allocated for the
	 * rawcb.
	 */
	if (rp == 0)
		return (-ENOBUFS);
	if ((error = soreserve(so, raw_sendspace, raw_recvspace)) != 0)	//set send and receive high water to 8192
		return (error);
	rp->rcb_socket = so;
	rp->rcb_proto.sp_family = so->so_proto->pr_domain->dom_family;
	rp->rcb_proto.sp_protocol = proto;
	insque(rp, &rawcb);
	return (0);
}
/*
 * Detach the raw connection block and discard
 * socket resources.
 */
void
raw_detach(struct rawcb *rp)
{
	struct socket *so = rp->rcb_socket;

	so->so_pcb = 0;
	sofree(so);
	remque(rp);
#ifdef notdef
	if (rp->rcb_laddr)
		m_freem(dtom(rp->rcb_laddr));
	rp->rcb_laddr = 0;
#endif
	kfree((caddr_t)(rp));
}

/*
 * Disconnect and possibly release resources.
 */
void
raw_disconnect(struct rawcb *rp)
{

#ifdef notdef
	if (rp->rcb_faddr)
		m_freem(dtom(rp->rcb_faddr));
	rp->rcb_faddr = 0;
#endif
	if (rp->rcb_socket->so_state & SS_NOFDREF)
		raw_detach(rp);
}
