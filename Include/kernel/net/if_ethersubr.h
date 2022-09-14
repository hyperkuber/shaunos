/*
 * if_ethersubr.h
 *
 *  Created on: Jun 21, 2013
 *      Author: root
 */

#ifndef SHAUN_IF_ETHERSUBR_H_
#define SHAUN_IF_ETHERSUBR_H_


#include <kernel/net/if.h>
#include <kernel/net/if_ether.h>
#include <kernel/mbuf.h>


int ether_input(struct ifnet *ifp, struct ether_header *et, struct mbuf *m);

int ether_output(struct ifnet *ifp, struct mbuf * m, struct sockaddr *dst, struct rtentry *rt0);


#endif /* SHAUN_IF_ETHERSUBR_H_ */
