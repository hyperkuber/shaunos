/*
 * udp.h
 *
 *  Created on: Aug 26, 2013
 *      Author: root
 */

#ifndef SHAUN_UDP_H_
#define SHAUN_UDP_H_


#include <kernel/types.h>

struct udphdr {
	u16	uh_sport;
	u16	uh_dport;
	s16	uh_ulen;
	u16	uh_sum;
};



















#endif /* SHAUN_UDP_H_ */
