/*
 * ip.h
 *
 *  Created on: Jun 27, 2013
 *      Author: root
 */

#ifndef SHAUN_IP_H_
#define SHAUN_IP_H_

#include <kernel/types.h>
#include <arpa/inet.h>

#define IPVERSION	4

/*
 * ip header struct, only for little endian arc.
 *
 */
struct ip {
	u8	ip_hl:4;	// header length
	u8	ip_v:4;		//version
	u8	ip_tos;		//type of service
	s16	ip_len;		//total length
	u16	ip_id;		//identification
	s16	ip_off;		//fragment offset
#define IP_DF	0x4000	//dont fragment
#define IP_MF	0x2000	//more fragment
#define IP_OFFMASK	0x1fff	//mask for fragment bits
	u8	ip_ttl;		//ttl
	u8	ip_p;	//Protocol
	u16	ip_sum;		//check sum
	struct in_addr	ip_src, ip_dst;
};


#define	IP_MAXPACKET	65535		/* maximum packet size */

/*
 * Definitions for IP type of service (ip_tos)
 */
#define	IPTOS_LOWDELAY		0x10
#define	IPTOS_THROUGHPUT	0x08
#define	IPTOS_RELIABILITY	0x04

/*
 * Internet implementation parameters.
 */
#define	MAXTTL		255		/* maximum time to live (seconds) */
#define	IPDEFTTL	64		/* default ttl, from RFC 1340 */
#define	IPFRAGTTL	60		/* time to live for frags, slowhz */
#define	IPTTLDEC	1		/* subtracted when forwarding */

#define	IP_MSS		576		/* default maximum segment size */



/*
 * Definitions for options.
 */
#define	IPOPT_COPIED(o)		((o)&0x80)
#define	IPOPT_CLASS(o)		((o)&0x60)
#define	IPOPT_NUMBER(o)		((o)&0x1f)

#define	IPOPT_CONTROL		0x00
#define	IPOPT_RESERVED1		0x20
#define	IPOPT_DEBMEAS		0x40
#define	IPOPT_RESERVED2		0x60

#define	IPOPT_EOL		0		/* end of option list */
#define	IPOPT_NOP		1		/* no operation */

#define	IPOPT_RR		7		/* record packet route */
#define	IPOPT_TS		68		/* timestamp */
#define	IPOPT_SECURITY		130		/* provide s,c,h,tcc */
#define	IPOPT_LSRR		131		/* loose source route */
#define	IPOPT_SATID		136		/* satnet id */
#define	IPOPT_SSRR		137		/* strict source route */


/*
 * Offsets to fields in options other than EOL and NOP.
 */
#define	IPOPT_OPTVAL		0		/* option ID */
#define	IPOPT_OLEN		1		/* option length */
#define	IPOPT_OFFSET		2		/* offset within option */
#define	IPOPT_MINOFF		4		/* min value of above */

struct ip_timestamp {
	u8	ipt_code;
	u8	ipt_len;
	u8	ipt_ptr;
	u32	ipt_flg:4,
		ipt_oflw:4;
	union ipt_timestamp {
		n_time ipt_time[1];
		struct ipt_ta {
			struct in_addr ipt_addr;
			n_time ipt_time;
		} ipt_ta[1];
	} ipt_timestamp;
};


/* flag bits for ipt_flg */
#define	IPOPT_TS_TSONLY		0		/* timestamps only */
#define	IPOPT_TS_TSANDADDR	1		/* timestamps and addresses */
#define	IPOPT_TS_PRESPEC	3		/* specified modules only */



/*
 * This is the real IPv4 pseudo header, used for computing the TCP and UDP
 * checksums. For the Internet checksum, struct ipovly can be used instead.
 * For stronger checksums, the real thing must be used.
 */
struct ippseudo {
	struct in_addr ippseudo_src;	/* source internet address */
	struct in_addr ippseudo_dst;	/* destination internet address */
	u8  ippseudo_pad;		/* pad, must be zero */
	u8  ippseudo_p;		/* protocol */
	u16 ippseudo_len;		/* protocol length */
};

#endif /* SHAUN_IP_H_ */
