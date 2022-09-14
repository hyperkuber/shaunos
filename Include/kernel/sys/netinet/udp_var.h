/*
 * udp_val.h
 *
 *  Created on: Aug 26, 2013
 *      Author: root
 */

#ifndef SHAUN_UDP_VAL_H_
#define SHAUN_UDP_VAL_H_

#include <kernel/sys/netinet/ip_val.h>


struct udpiphdr	{
	struct ipovly ui_i;
	struct udphdr ui_u;
};


#define ui_next	ui_i.ih_next
#define ui_prev	ui_i.ih_prev
#define ui_x1	ui_i.ih_x1
#define ui_pr	ui_i.ih_pr
#define ui_len	ui_i.ih_len
#define ui_src	ui_i.ih_src
#define ui_dst	ui_i.ih_dst
#define ui_sport	ui_u.uh_sport
#define ui_dport	ui_u.uh_dport
#define ui_ulen	ui_u.uh_ulen
#define ui_sum	ui_u.uh_sum

struct udpstat {
	u32	udps_ipackets;
	u32	udps_hdrops;
	u32	udps_badsum;
	u32	udps_badlen;
	u32	udps_noport;
	u32	udps_noportbcast;
	u32	udps_fullsock;
	u32	udpps_pcbcachemiss;
	u32	udps_opackets;
};



extern struct inpcb udb;
extern struct udpstat udpstat;

#define UDPCTL_CHECKSUM	1

extern int
udp_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr, struct mbuf *control);
extern void
udp_input(struct mbuf *m, int iphlen);
extern void
udp_init();
extern void
udp_ctlinput(int cmd, struct sockaddr *sa, struct ip *ip);

#endif /* SHAUN_UDP_VAL_H_ */
