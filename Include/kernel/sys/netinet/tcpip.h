/*
 * tcpip.h
 *
 *  Created on: Sep 21, 2013
 *      Author: root
 */

#ifndef TCPIP_H_
#define TCPIP_H_


#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/netinet/tcp.h>


struct tcpiphdr	{
	struct ipovly	ti_i;
	struct tcphdr	ti_t;
};

#define ti_next	ti_i.ih_next
#define ti_prev	ti_i.ih_prev
#define ti_x1	ti_i.ih_x1
#define ti_pr	ti_i.ih_pr
#define ti_len	ti_i.ih_len
#define ti_src	ti_i.ih_src
#define ti_dst 	ti_i.ih_dst
#define ti_sport	ti_t.th_sport
#define ti_dport	ti_t.th_dport
#define ti_seq	ti_t.th_seq
#define ti_ack	ti_t.th_ack
#define ti_x2	ti_t.th_x2
#define	ti_off	ti_t.th_off
#define ti_flags	ti_t.th_flags
#define ti_win	ti_t.th_win
#define	ti_sum	ti_t.th_sum
#define	ti_urp	ti_t.th_urp




#endif /* TCPIP_H_ */
