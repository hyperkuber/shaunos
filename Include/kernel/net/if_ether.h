/*
 * if_ether.h
 *
 *  Created on: Jun 17, 2013
 *      Author: root
 */

#ifndef SHAUN_IF_ETHER_H_
#define SHAUN_IF_ETHER_H_


#include <kernel/types.h>
#include <kernel/net/if.h>
#include <list.h>
#include <arpa/inet.h>
#include <kernel/net/if_arp.h>




#define ETHER_ADDR_LEN	6
#define ETH_MTU	1500
#define ETHER_MAX_LEN	1518

#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806
#define ETHERTYPE_REVARP	0x8035	/* reverse Addr. resolution protocol */

#define ETH_FCS_LEN 4
#define ETH_ALEN        6               /* Octets in one ethernet addr   */
#define ETH_HLEN        14              /* Total octets in header.       */
#define ETH_ZLEN        60              /* Min. octets in frame sans FCS */
#define ETH_DATA_LEN    1500            /* Max. octets in payload        */
#define ETH_FRAME_LEN   1514            /* Max. octets in frame sans FCS */

#define ETHERNET_IEEE_VLAN_TYPE 0x8100  /* 802.3ac packet */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

struct ether_multi;

struct arpcom {
	struct ifnet ac_if;
	u8 ac_enaddr[ETHER_ADDR_LEN];
	struct in_addr ac_ipaddr;
	struct ether_multi *ac_multiaddrs;
	int ac_multicnt;
};


struct ether_multi {
	u8 enm_addrlo[ETHER_ADDR_LEN]; /* low  or only address of range */
	u8 enm_addrhi[ETHER_ADDR_LEN]; /* high or only address of range */
	struct	 arpcom *enm_ac;	/* back pointer to arpcom */
	u32	 enm_refcount;		/* no. claims to this addr/range */
	struct linked_list enm_list;
};


struct ether_header {
	u8	dest_mac[ETHER_ADDR_LEN];
	u8	src_mac[ETHER_ADDR_LEN];
	u16	eth_type;
};


struct _le_softc {
	struct arpcom sc_ac;
#define sc_if sc_ac.ac_if
#define sc_addr	sc_ac.ac_enaddr
	void *iomem;
	u32 sc_runt;
};


struct ether_arp {
	struct arphdr	ea_hdr;
	u8	arp_sha[6];				//sender hardware address
	u8	arp_spa[4];				//sender protocol address	(ip address)
	u8	arp_tha[6];				//target hardware address
	u8	arp_tpa[4];				//target protocol address
};

#define arp_hrd	ea_hdr.ar_hrd
#define arp_pro	ea_hdr.ar_pro
#define arp_hln	ea_hdr.ar_hln
#define arp_pln	ea_hdr.ar_pln
#define arp_op	ea_hdr.ar_op



struct llinfo_arp {
	struct llinfo_arp *la_next;
	struct llinfo_arp *la_prev;
	struct rtentry	*la_rt;
	struct mbuf *la_hold;
	long	la_asked;
};
#define la_timer la_rt->rt_rmx.rmx_expire


struct sockaddr_inarp {
	u8	sin_len;
	u8	sin_family;
	u16	sin_port;
	struct in_addr sin_addr;
	struct in_addr sin_srcaddr;
	u16	sin_tos;
	u16	sin_other;
};
#define SIN_PROXY 1

/*
 * IP and ethernet specific routing flags
 */
#define	RTF_USETRAILERS	RTF_PROTO1	/* use trailers */
#define RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */

/*
 * Macro to map an IP multicast address to an Ethernet multicast address.
 * The high-order 25 bits of the Ethernet address are statically assigned,
 * and the low-order 23 bits are taken from the low end of the IP address.
 */
#define ETHER_MAP_IP_MULTICAST(ipaddr, enaddr) \
	/* struct in_addr *ipaddr; */ \
	/* u_char enaddr[6];	   */ \
{ \
	(enaddr)[0] = 0x01; \
	(enaddr)[1] = 0x00; \
	(enaddr)[2] = 0x5e; \
	(enaddr)[3] = ((u8 *)ipaddr)[1] & 0x7f; \
	(enaddr)[4] = ((u8 *)ipaddr)[2]; \
	(enaddr)[5] = ((u8 *)ipaddr)[3]; \
}



int
arpresolve(struct arpcom *ac,
		struct rtentry *rt,
		struct  mbuf *m,
		struct sockaddr *dst, u8 *desten);

extern void
arp_rtrequest(int req, struct rtentry *rt, struct sockaddr *sa);
extern int leinit();
extern int leread(int unit, char *buf, int len);
#endif /* SHAUN_IF_ETHER_H_ */
