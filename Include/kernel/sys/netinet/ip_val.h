/*
 * ip_val.h
 *
 *  Created on: Jun 27, 2013
 *      Author: root
 */

#ifndef SHAUN_IP_VAL_H_
#define SHAUN_IP_VAL_H_



#include <kernel/types.h>
#include <kernel/net/if.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/sys/socket.h>
#include <kernel/net/route.h>

struct	ipstat {
	u32	ips_total;		/* total packets received */
	u32	ips_badsum;		/* checksum bad */
	u32	ips_tooshort;		/* packet too short */
	u32	ips_toosmall;		/* not enough data */
	u32	ips_badhlen;		/* ip header length < data size */
	u32	ips_badlen;		/* ip length < ip header length */
	u32	ips_fragments;		/* fragments received */
	u32	ips_fragdropped;	/* frags dropped (dups, out of space) */
	u32	ips_fragtimeout;	/* fragments timed out */
	u32	ips_forward;		/* packets forwarded */
	u32	ips_cantforward;	/* packets rcvd for unreachable dest */
	u32	ips_redirectsent;	/* packets forwarded on same net */
	u32	ips_noproto;		/* unknown or unsupported protocol */
	u32	ips_delivered;		/* datagrams delivered to upper level*/
	u32	ips_localout;		/* total ip packets generated here */
	u32	ips_odropped;		/* lost packets due to nobufs, etc. */
	u32	ips_reassembled;	/* total packets reassembled ok */
	u32	ips_fragmented;		/* datagrams successfully fragmented */
	u32	ips_ofragments;		/* output fragments created */
	u32	ips_cantfrag;		/* don't fragment flag was set, etc. */
	u32	ips_badoptions;		/* error in option processing */
	u32	ips_noroute;		/* packets discarded due to no route */
	u32	ips_badvers;		/* ip version != 4 */
	u32	ips_rawout;		/* total raw ip packets generated */
	u32	ips_badfrags;		/* malformed fragments (bad length) */
	u32	ips_rcvmemdrop;		/* frags dropped for lack of memory */
	u32	ips_toolong;		/* ip length > max ip packet size */
	u32	ips_nogif;		/* no match gif found */
	u32	ips_badaddr;		/* invalid address on header */
	u32	ips_inhwcsum;		/* hardware checksummed on input */
	u32	ips_outhwcsum;		/* hardware checksummed on output */
	u32	ips_notmember;		/* multicasts for unregistered groups */
};


/* flags passed to ip_output as last parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_RAWOUTPUT		0x2		/* raw ip header exists */
#define	IP_ROUTETOIF		SO_DONTROUTE	/* bypass routing tables */
#define	IP_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */

/*
 * DVMRP-specific setsockopt commands.
 */
#define	DVMRP_INIT	100
#define	DVMRP_DONE	101
#define	DVMRP_ADD_VIF	102
#define	DVMRP_DEL_VIF	103
#define	DVMRP_ADD_LGRP	104
#define	DVMRP_DEL_LGRP	105
#define	DVMRP_ADD_MRT	106
#define	DVMRP_DEL_MRT	107


/*
 * Structure attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 */
struct ip_moptions {
	struct	ifnet *imo_multicast_ifp; /* ifp for outgoing multicasts */
	u8	imo_multicast_ttl;	/* TTL for outgoing multicasts */
	u8	imo_multicast_loop;	/* 1 => hear sends if a member */
	u16	imo_num_memberships;	/* no. memberships this socket */
	struct	in_multi *imo_membership[IP_MAX_MEMBERSHIPS];
};





/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN	40


struct ipoption {
	struct in_addr	ipopt_dst;
	char ipopt_list[MAX_IPOPTLEN];
};


struct ipasfrag {
	u8	ip_hl:4;
	u8	ip_v:4;
	u8	ipf_mff;
	s16	ip_len;
	u16	ip_id;
	s16	ip_off;
	u8	ip_ttl;
	u8	ip_p;
	u16	ip_sum;
	struct ipasfrag *ipf_next;
	struct ipasfrag *ipf_prev;
};

struct ipqent {
	struct ipqent *next, *prev;
	struct ip *ipqe_ip;
	struct mbuf *ipqe_m;
	u8	ipq_mff;
};

struct ipqehead {
	struct ipqent *first;
};


struct ipq {
	struct ipq *next, *prev;
	u8	ipq_ttl;
	u8	ipq_p;
	u16	ipq_id;
	struct ipqehead ipq_fragq;
	struct in_addr	ipq_src, ipq_dst;
};

struct ipovly {
	caddr_t ih_next, ih_prev;
	u8	ih_x1;
	u8	ih_pr;
	s16	ih_len;
	struct in_addr ih_src;
	struct in_addr ih_dst;
};

extern struct ipq ipq;
extern int	ip_defttl;
extern struct 	ipstat	ipstat;

extern void
rip_init();
extern void
rip_input(struct mbuf *m);
extern int
rip_output(struct mbuf *m, struct socket *so, u32 dst);
extern int
rip_ctloutput(int op, struct socket *so, int level, int optname, struct mbuf **m);
extern int
rip_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control);

extern void
rip_input(struct mbuf *m);
extern void
ip_freemoptions(struct ip_moptions *imo);
extern int
ip_output(struct mbuf *m0, struct mbuf *opt,
		struct route *ro, int flags,
		struct ip_moptions *imo);

extern int
ip_ctloutput(int op, struct socket *so, int level, int optname, struct mbuf **mp);

extern void
ip_stripoptions(struct mbuf *m, struct mbuf *mopt);
extern struct mbuf *
ip_srcroute();

extern void
ip_drain();
extern void
ip_slowtimo();
extern void
ip_init();
#endif /* SHAUN_IP_VAL_H_ */
