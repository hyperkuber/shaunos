/*
 * in_pcb.h
 *
 *  Created on: Aug 19, 2013
 *      Author: root
 */

#ifndef SHAUN_IN_PCB_H_
#define SHAUN_IN_PCB_H_

#include <kernel/types.h>
#include <kernel/mbuf.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/socketval.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/net/route.h>
/*
 * note:faddr,fport,laddr,lport all these
 * values stored here are in network byte order
 */
struct inpcb {
	struct inpcb *inp_next, *inp_prev;
	struct inpcb *inp_head;
	struct in_addr inp_faddr;	//foregin ip address
	u16	inp_fport;				//foregin port
	struct in_addr inp_laddr;	//local ip address
	u16	inp_lport;				//local port
	struct socket * inp_socket;
	caddr_t inp_ppcb;
	struct route inp_route;		//routing entry
	int inp_flags;
	struct ip inp_ip;			//header prototype;
	struct mbuf *inp_options;	//ip options
	struct ip_moptions *inp_moptions;	//ip multicast options
};



/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR)
#define	INP_HDRINCL		0x08	/* user supplies entire IP header , for raw socket*/

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb);

extern int
in_pcbconnect(struct inpcb *inp, struct mbuf *nam);
extern void
in_pcbdisconnect(struct inpcb *inp);
extern struct inpcb *
in_pcblookup(struct inpcb *head, struct in_addr faddr,
		u32 fport_arg, struct in_addr laddr, u32 lport_arg,
		int flags);
extern void
in_pcbnotify(struct inpcb *head, struct sockaddr *dst,
		u32	fport_arg, struct in_addr laddr, u32 lport_arg,
		int cmd, void (*notify)(struct inpcb *, int));
extern int
in_pcbdetach(struct inpcb *inp);
extern int
in_pcbbind(struct inpcb *inp, struct mbuf *nam);
extern void
in_setsockaddr(struct inpcb *inp, struct mbuf *nam);
extern void
in_setpeeraddr(struct inpcb *inp, struct mbuf *nam);
extern int
in_pcballoc(struct socket *so, struct inpcb *head);
extern void
in_losing(struct inpcb *inp);

#endif /* SHAUN_IN_PCB_H_ */
