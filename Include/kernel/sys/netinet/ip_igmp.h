/*
 * ip_igmp.h
 *
 *  Created on: Nov 21, 2013
 *      Author: root
 */

#ifndef SHAUN_IP_IGMP_H_
#define SHAUN_IP_IGMP_H_


#include <kernel/types.h>
#include <arpa/inet.h>
#include <kernel/sys/protosw.h>

struct igmp {
	u8		igmp_type;	/* version & type of IGMP message  */
	u8		igmp_code;	/* unused, should be zero          */
	u16		igmp_cksum;	/* IP-style checksum               */
	struct in_addr	igmp_group;	/* group address being reported    */
};					/*  (zero for queries)             */

#define IGMP_MINLEN		     8

#define IGMP_HOST_MEMBERSHIP_QUERY   0x11  /* message types, incl. version */
#define IGMP_HOST_MEMBERSHIP_REPORT  0x12
#define IGMP_DVMRP		     0x13  /* for experimental multicast   */
					   /*  routing protocol            */

#define IGMP_MAX_HOST_REPORT_DELAY   10    /* max delay for response to    */


struct igmpstat {
	u32	igps_rcv_total;		/* total IGMP messages received */
	u32	igps_rcv_tooshort;	/* received with too few bytes */
	u32	igps_rcv_badsum;	/* received with bad checksum */
	u32	igps_rcv_queries;	/* received membership queries */
	u32	igps_rcv_badqueries;	/* received invalid queries */
	u32	igps_rcv_reports;	/* received membership reports */
	u32	igps_rcv_badreports;	/* received invalid reports */
	u32	igps_rcv_ourreports;	/* received reports for our groups */
	u32	igps_snd_reports;	/* sent membership reports */
};

extern struct igmpstat igmpstat;

/*
 * Macro to compute a random timer value between 1 and (IGMP_MAX_REPORTING_
 * DELAY * countdown frequency).  We generate a "random" number by adding
 * the total number of IP packets received, our primary IP address, and the
 * multicast address being timed-out.  The 4.3 random() routine really
 * ought to be available in the kernel!
 */
#define IGMP_RANDOM_DELAY(multiaddr) \
	/* struct in_addr multiaddr; */ \
	( (ipstat.ips_total + \
	   ntohl(IA_SIN(in_ifaddr)->sin_addr.s_addr) + \
	   ntohl((multiaddr).s_addr) \
	  ) \
	  % (IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ) + 1 \
	)




extern void
igmp_init();
extern void
igmp_input(struct mbuf *m, int iphlen);
extern void
igmp_fasttimo();


#endif /* SHAUN_IP_IGMP_H_ */
