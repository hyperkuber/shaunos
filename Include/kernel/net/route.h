/*
 * route.h
 *
 *  Created on: Jul 11, 2013
 *      Author: root
 */

#ifndef SHAUN_ROUTE_H_
#define SHAUN_ROUTE_H_


#include <kernel/types.h>
#include <kernel/net/if.h>
#include <kernel/sys/socket.h>
#include <kernel/net/radix.h>








/*
 * Routing statistics.
 */
struct	rtstat {
	short	rts_badredirect;	/* bogus redirect calls */
	short	rts_dynamic;		/* routes created by redirects */
	short	rts_newgateway;		/* routes modified by redirects */
	short	rts_unreach;		/* lookups which failed */
	short	rts_wildcard;		/* lookups satisfied by a wildcard */
};


struct route {
	struct rtentry *ro_rt;
	struct sockaddr ro_dst;
};


/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_kmetrics {
	u32		rmx_pksent;	/* packets sent using this route */
	u32		rmx_locks;	/* Kernel must leave these values */
	u32		rmx_mtu;	/* MTU for this path */
	u32		rmx_expire;	/* lifetime for route, e.g. redirect */
	u32		rmx_pad;
};
/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_metrics {
	u32	rmx_locks;	/* Kernel must leave these values alone */
	u32	rmx_mtu;	/* MTU for this path */
	u32	rmx_hopcount;	/* max hops expected */
	u32	rmx_expire;	/* lifetime for route, e.g. redirect */
	u32	rmx_recvpipe;	/* inbound delay-bandwith product */
	u32	rmx_sendpipe;	/* outbound delay-bandwith product */
	u32	rmx_ssthresh;	/* outbound gateway buffer limit */
	u32	rmx_rtt;	/* estimated round trip time */
	u32	rmx_rttvar;	/* estimated rtt variance */
	u32	rmx_pksent;	/* packets sent using this route */
};

struct rtentry {
	struct radix_node rt_nodes[2];
#define	rt_key(r)	((struct sockaddr *)((r)->rt_nodes->rn_key))
#define	rt_mask(r)	((struct sockaddr *)((r)->rt_nodes->rn_mask))
	struct sockaddr *rt_gateway;
	u32 rt_flags;
	s32	rt_refcnt;
	u32	rt_use;
	struct ifnet *rt_ifp;
	struct ifaddr *rt_ifa;
	struct sockaddr *rt_genmask;
	caddr_t	rt_llinfo;
	struct rt_metrics rt_rmx;
	struct rtentry * rt_gwroute;
	struct rtentry *rt_parent;
	u16	rt_labelid;
	u8	rt_priority;
};




#define	RTF_UP		0x1		/* route usable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_REJECT	0x8		/* host or net unreachable */
#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define	RTF_MODIFIED	0x20		/* modified dynamically (by redirect) */
#define RTF_DONE	0x40		/* message confirmed */
#define RTF_MASK	0x80		/* subnet mask present */
#define RTF_CLONING	0x100		/* generate new routes on use */
#define RTF_XRESOLVE	0x200		/* external daemon resolves name */
#define RTF_LLINFO	0x400		/* generated by ARP or ESIS */
#define RTF_STATIC	0x800		/* manually added */
#define RTF_BLACKHOLE	0x1000		/* just discard pkts (during updates) */
#define RTF_PROTO2	0x4000		/* protocol specific routing flag */
#define RTF_PROTO1	0x8000		/* protocol specific routing flag */





/*
 * Structures for routing messages.
 */
struct rt_msghdr {
	u16	rtm_msglen;	/* to skip over non-understood messages */
	u8	rtm_version;	/* future binary compatibility */
	u8	rtm_type;	/* message type */
	u16	rtm_index;	/* index for associated ifp */
	int	rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
	pid_t	rtm_pid;	/* identify sender */
	int	rtm_seq;	/* for sender to identify action */
	int	rtm_errno;	/* why failed */
	int	rtm_use;	/* from rtentry */
	u32	rtm_inits;	/* which metrics we are initializing */
	struct	rt_metrics rtm_rmx; /* metrics themselves */
};


#define RTM_VERSION	3	/* Up the ante and ignore older versions */

#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#define RTM_LOSING	0x5	/* Kernel Suspects Partitioning */
#define RTM_REDIRECT	0x6	/* Told to use different route */
#define RTM_MISS	0x7	/* Lookup failed on this address */
#define RTM_LOCK	0x8	/* fix specified metrics */
#define RTM_OLDADD	0x9	/* caused by SIOCADDRT */
#define RTM_OLDDEL	0xa	/* caused by SIOCDELRT */
#define RTM_RESOLVE	0xb	/* req to resolve dst to LL addr */
#define RTM_NEWADDR	0xc	/* address being added to iface */
#define RTM_DELADDR	0xd	/* address being removed from iface */
#define RTM_IFINFO	0xe	/* iface going up/down etc. */

#define RTV_MTU		0x1	/* init or lock _mtu */
#define RTV_HOPCOUNT	0x2	/* init or lock _hopcount */
#define RTV_EXPIRE	0x4	/* init or lock _hopcount */
#define RTV_RPIPE	0x8	/* init or lock _recvpipe */
#define RTV_SPIPE	0x10	/* init or lock _sendpipe */
#define RTV_SSTHRESH	0x20	/* init or lock _ssthresh */
#define RTV_RTT		0x40	/* init or lock _rtt */
#define RTV_RTTVAR	0x80	/* init or lock _rttvar */
/*
 * Bitmask values for rtm_addr.
 */
#define RTA_DST		0x1	/* destination sockaddr present */
#define RTA_GATEWAY	0x2	/* gateway sockaddr present */
#define RTA_NETMASK	0x4	/* netmask sockaddr present */
#define RTA_GENMASK	0x8	/* cloning mask sockaddr present */
#define RTA_IFP		0x10	/* interface name sockaddr present */
#define RTA_IFA		0x20	/* interface addr sockaddr present */
#define RTA_AUTHOR	0x40	/* sockaddr for author of redirect */
#define RTA_BRD		0x80	/* for NEWADDR, broadcast or p-p dest addr */


/*
 * Index offsets for sockaddr array for alternate internal encoding.
 */
#define RTAX_DST	0	/* destination sockaddr present */
#define RTAX_GATEWAY	1	/* gateway sockaddr present */
#define RTAX_NETMASK	2	/* netmask sockaddr present */
#define RTAX_GENMASK	3	/* cloning mask sockaddr present */
#define RTAX_IFP	4	/* interface name sockaddr present */
#define RTAX_IFA	5	/* interface addr sockaddr present */
#define RTAX_AUTHOR	6	/* sockaddr for author of redirect */
#define RTAX_BRD	7	/* for NEWADDR, broadcast or p-p dest addr */
#define RTAX_MAX	8	/* size of array to allocate */


/*
 * rmx_rtt and rmx_rttvar are stored as microseconds;
 * RTTTOPRHZ(rtt) converts to a value suitable for use
 * by a protocol slowtimo counter.
 */
#define	RTM_RTTUNIT	1000000	/* units for rtt, rttvar, as units per sec */
#define	RTTTOPRHZ(r)	((r) / (RTM_RTTUNIT / PR_SLOWHZ))


struct rt_addrinfo {
	int	rti_addrs;
	struct	sockaddr *rti_info[RTAX_MAX];
};



struct route_cb {
	int	ip_count;
	int	ns_count;
	int	iso_count;
	int	any_count;
} route_cb;


#define Bcmp(a, b, n) memcmp(((void *)(a)), ((void *)(b)), (n))
#define Bcopy(a, b, n) memcpy(((void *)(b)), ((void *)(a)), (unsigned)(n))
#define Bzero(p, n) memset((void *)(p), 0, (size_t)(n));
#define R_Malloc(p, t, n) (p = (t) kmalloc((size_t)(n)))
#define Free(p) kfree((char *)p);

#define equal(x, y) (Bcmp((caddr_t)(x), (caddr_t)(y), (x)->sa_len) == 0)



#define	RTFREE(rt) \
	if ((rt)->rt_refcnt <= 1) \
		rtfree(rt); \
	else \
		(rt)->rt_refcnt--;

struct	radix_node_head *rt_tables[AF_MAX+1];
extern void
route_init();
extern struct rtentry *
rtalloc1(struct sockaddr *dst, int report);
extern void
rtalloc(struct route *ro);
extern void
rtfree(struct rtentry *rt);
extern  void
rtredirect(struct sockaddr *dst,
		struct sockaddr *gateway,
		struct sockaddr *netmask,
		int	flags,
		struct sockaddr *src,
		struct rtentry **rtp);

extern void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error);
extern void
rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt);
extern int
rtrequest(int req,
		struct sockaddr *dst,
		struct sockaddr *gateway,
		struct sockaddr *netmask,
		int flags,
		struct rtentry **ret_nrt);
extern int
rt_setgate(struct rtentry *rt0, struct sockaddr *dst,
		struct sockaddr *gate);
extern struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr	*dst,
		struct sockaddr	*gateway);
extern void
ifafree(struct ifaddr *ifa);
void
rt_ifmsg(struct ifnet *ifp);
extern int
rtinit(struct ifaddr *ifa, int cmd, int flags);

#endif /* SHAUN_ROUTE_H_ */
