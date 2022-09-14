/*
 * if.h
 *
 *  Created on: Jun 14, 2013
 *      Author: root
 */

#ifndef SHAUN_IF_H_
#define SHAUN_IF_H_


#include <kernel/types.h>
#include <kernel/time.h>
#include <kernel/mbuf.h>
#include <kernel/sys/socket.h>
#include <kernel/sys/socketval.h>


struct ifnet;
struct rtentry;


struct ifaddr {
	struct ifaddr *ifa_next;
	struct ifnet *ifa_ifp;
	struct sockaddr *ifa_addr;
	struct sockaddr *ifa_dstaddr;
#define ifa_broadaddr	ifa_dstaddr
	struct sockaddr *ifa_netmask;
	void (*ifa_rtrequest)();
	u16 ifa_flags;
	short ifa_refcnt;
	int ifa_metric;
};

struct ifqueue {
	struct mbuf *ifq_head;
	struct mbuf *ifq_tail;
	int ifq_len;
	int ifq_maxlen;
	int ifq_drops;
};


struct ifnet {
	struct ifnet *if_next;
	struct ifaddr *if_addrlist;
	char *if_name;
	short if_unit;
	u16 if_index;
	short if_flags;
	short if_timer;
	int if_pcount;
	caddr_t if_bpf;
	struct if_data {
		u8 ifi_type;
		u8 ifi_addrlen;
		u8 ifi_hdrlen;
		u32 ifi_mtu;
		u32 ifi_metric;
		u32 if_baudrate;
		u32 ifi_ipackets;
		u32 ifi_ierrors;
		u32 ifi_opackets;
		u32 ifi_oerrors;
		u32 ifi_collisions;
		u32 ifi_ibytes;
		u32 ifi_obytes;
		u32 ifi_imcasts;
		u32 ifi_omcasts;
		u32 ifi_iqdrops;
		u32 ifi_noproto;
		struct timeval ifi_lastchange;
	} if_data;

	int (*if_init)(int);
	int (*if_output)(struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *);
	int (*if_start)(struct ifnet *);
	int (*if_done)(struct ifnet *);
	int (*if_ioctl)(struct ifnet *, int , caddr_t );
	int (*if_reset)(int);
	int (*if_watchdog)(int);

	struct ifqueue if_snd;
	void *if_softc;
};

#define if_mtu	if_data.ifi_mtu
#define if_type	if_data.ifi_type
#define if_addrlen	if_data.ifi_addrlen
#define if_hdrlen	if_data.ifi_hdrlen
#define if_metric	if_data.ifi_metric
#define if_baudrate	if_data.ifi_baudrate

#define if_ipackets	if_data.ifi_ipackets
#define if_ierrors	if_data.ifi_ierrors
#define if_opackets	if_data.ifi_opackets
#define if_oerrors	if_data.ifi_oerrors
#define if_collisions	if_data.ifi_collisions
#define if_ibytes	if_data.ifi_ibytes
#define if_obytes	if_data.ifi_obytes
#define if_imcasts	if_data.ifi_imcasts
#define if_omcasts	if_data.ifi_omcasts
#define if_iqdrops	if_data.ifi_qdrops
#define if_noproto	if_data.ifi_noproto
#define if_lastchange	if_data.ifi_lastchange

#define	IFF_UP		0x1		/* interface is up */
#define	IFF_BROADCAST	0x2		/* broadcast address valid */
#define	IFF_DEBUG	0x4		/* turn on debugging */
#define	IFF_LOOPBACK	0x8		/* is a loopback net */
#define	IFF_POINTOPOINT	0x10		/* interface is point-to-point link */
#define	IFF_NOTRAILERS	0x20		/* avoid use of trailers */
#define	IFF_RUNNING	0x40		/* resources allocated */
#define	IFF_NOARP	0x80		/* no address resolution protocol */
#define	IFF_PROMISC	0x100		/* receive all packets */
#define	IFF_ALLMULTI	0x200		/* receive all multicast packets */
#define	IFF_OACTIVE	0x400		/* transmission in progress */
#define	IFF_SIMPLEX	0x800		/* can't hear own transmissions */
#define	IFF_LINK0	0x1000		/* per link layer defined bit */
#define	IFF_LINK1	0x2000		/* per link layer defined bit */
#define	IFF_LINK2	0x4000		/* per link layer defined bit */
#define	IFF_MULTICAST	0x8000		/* supports multicast */
#define IFF_CHKSUM_OFFLOAD	0x10000

/* flags set internally only: */
#define	IFF_CANTCHANGE \
	(IFF_BROADCAST|IFF_POINTOPOINT|IFF_RUNNING|IFF_OACTIVE|\
	    IFF_SIMPLEX|IFF_MULTICAST|IFF_ALLMULTI)

#define	IFA_ROUTE	RTF_UP		/* route installed */

/*
 * Message format for use in obtaining information about interfaces
 * from getkerninfo and the routing socket
 */
struct if_msghdr {
	u16	ifm_msglen;	/* to skip over non-understood messages */
	u8	ifm_version;	/* future binary compatability */
	u8	ifm_type;	/* message type */
	int	ifm_addrs;	/* like rtm_addrs */
	int	ifm_flags;	/* value of if_flags */
	u16	ifm_index;	/* index for associated ifp */
	struct	if_data ifm_data;/* statistics and other data about if */
};

/*
 * Message format for use in obtaining information about interface addresses
 * from getkerninfo and the routing socket
 */
struct ifa_msghdr {
	u16	ifam_msglen;	/* to skip over non-understood messages */
	u8	ifam_version;	/* future binary compatability */
	u8	ifam_type;	/* message type */
	int	ifam_addrs;	/* like rtm_addrs */
	int	ifam_flags;	/* value of ifa_flags */
	u16	ifam_index;	/* index for associated ifp */
	int	ifam_metric;	/* value of ifa_metric */
};




/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
#define	IFNAMSIZ	16
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		caddr_t	ifru_data;
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define	ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
};

struct ifaliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr ifra_addr;
	struct	sockaddr ifra_broadaddr;
	struct	sockaddr ifra_mask;
};


/*
 * Structure used in SIOCGIFCONF request.
 * Used to retrieve interface configuration
 * for machine (useful for programs which
 * must know all networks accessible).
 */
struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		caddr_t	ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

/*
 * BSD style if queues
 */
#define IF_QFULL(ifq)	((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define IF_QDROP(ifq)	((ifq)->ifq_drops++)
#define IF_ENQUEUE(ifq, m) \
	do { \
		(m)->m_nextpkt = NULL;					\
		if ((ifq)->ifq_tail == NULL)		\
			(ifq)->ifq_head = (m);			\
		else								\
			(ifq)->ifq_tail->m_nextpkt = (m);	\
		(ifq)->ifq_tail = (m);				\
		(ifq)->ifq_len++;					\
	} while (0)

#define IF_DEQUEUE(ifq, m)	\
	do {	\
		(m) = (ifq)->ifq_head;				\
		if((m)){							\
			if (((ifq)->ifq_head = (m)->m_nextpkt) == NULL)	\
				(ifq)->ifq_tail = NULL;		\
			(m)->m_nextpkt = NULL;				\
			(ifq)->ifq_len--;				\
		}									\
	} while (0)

/*
 * max queue length,
 *  65535 / 1500
 */
#define ifq_maxlength	50

#define	IFQ_MAXLEN	50
#define	IFNET_SLOWHZ	1		/* granularity is 1 second */

#define	IFAFREE(ifa) \
	if ((ifa)->ifa_refcnt <= 0) \
		ifafree(ifa); \
	else \
		(ifa)->ifa_refcnt--;



extern int
if_attach(struct ifnet *ifp);
extern struct ifaddr *
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp);
extern struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr);
extern struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr);
extern struct ifaddr *
ifa_ifwithnet(struct sockaddr *addr);
extern void
ifinit();

extern int
ifioctl(struct socket *so, u32 cmd, caddr_t data);
extern int
ifconf(int cmd, caddr_t data);
extern struct ifnet *
ifunit(char *name);
extern void
if_down(struct ifnet *ifp);
extern void
if_up(struct ifnet *ifp);
#endif /* SHAUN_IF_H_ */
