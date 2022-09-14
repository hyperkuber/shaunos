/*
 * if_arp.h
 *
 *  Created on: Aug 16, 2013
 *      Author: root
 */

#ifndef SHAUN_IF_ARP_H_
#define SHAUN_IF_ARP_H_

#include <kernel/types.h>

struct arphdr {
	u16	ar_hrd;				//hardware type
	u16	ar_pro;				//protocol type
	u8	ar_hln;				//hardware address length
	u8	ar_pln;				//length of protocol address
	u16	ar_op;				//arp operation
};

#define ARPHRD_ETHER 	1	/* ethernet hardware format */
#define ARPHRD_FRELAY 	15	/* frame relay hardware format */

#define	ARPOP_REQUEST	1	/* request to resolve address */
#define	ARPOP_REPLY	2	/* response to previous request */
#define	ARPOP_REVREQUEST 3	/* request protocol address given hardware */
#define	ARPOP_REVREPLY	4	/* response giving protocol address */
#define ARPOP_INVREQUEST 8 	/* request to identify peer */
#define ARPOP_INVREPLY	9	/* response identifying peer */






#endif /* SHAUN_IF_ARP_H_ */
