/*
 * raw_cb.h
 *
 *  Created on: Aug 14, 2013
 *      Author: root
 */

#ifndef SHAUN_RAW_CB_H_
#define SHAUN_RAW_CB_H_

#include <kernel/mbuf.h>
#include <kernel/sys/socket.h>
#include <kernel/sys/socketval.h>

/*
 * Raw protocol interface control block.  Used
 * to tie a socket to the generic raw interface.
 */
struct rawcb {
	struct	rawcb *rcb_next;	/* doubly linked list */
	struct	rawcb *rcb_prev;
	struct	socket *rcb_socket;	/* back pointer to socket */
	struct	sockaddr *rcb_faddr;	/* destination address */
	struct	sockaddr *rcb_laddr;	/* socket's address */
	struct	sockproto rcb_proto;	/* protocol family, protocol */
};

#define	sotorawcb(so)		((struct rawcb *)(so)->so_pcb)

struct rawcb rawcb ;			/* head of list */

/*
 * Nominal space allocated to a raw socket.
 */
#define	RAWSNDQ		8192
#define	RAWRCVQ		8192

int
raw_attach(struct socket *so, int proto);
void
raw_detach(struct rawcb *rp);
void
raw_disconnect(struct rawcb *rp);

void
raw_init();

void
raw_input(struct mbuf *m0, struct sockproto *proto,
		struct sockaddr *src, struct sockaddr *dst);

int
raw_usrreq(struct socket *so, int req, struct mbuf *m,
		struct mbuf *nam, struct mbuf *control);
void
raw_ctlinput(int cmd, struct sockaddr *arg);

#endif /* SHAUN_RAW_CB_H_ */
