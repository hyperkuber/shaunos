/*
 * tcp.h
 *
 *  Created on: Sep 21, 2013
 *      Author: root
 */

#ifndef TCP_H_
#define TCP_H_

#include <kernel/types.h>


struct tcphdr {
	u16	th_sport;
	u16	th_dport;
	u32	th_seq;
	u32 th_ack;

	u8	 th_x2:4;
	u8	th_off:4;

	u8	th_flags;
	u16	th_win;
	u16	th_sum;
	u16	th_urp;
};

#define TH_FIN	0x01
#define TH_SYN	0x02
#define TH_RST	0x04
#define TH_PUSH	0x08
#define TH_ACK	0x10
#define TH_URG	0x20

#define TCPOPT_EOL	0
#define TCPOPT_NOP	1
#define TCPOPT_MAXSEG	2
#define 	TCPOLEN_MAXSEG	4
#define TCPOPT_WINDOW	3
#define 	TCPOLEN_WINDOW	3
#define TCPOPT_SACK_PERMITTED	4
#define 	TCPOLEN_SACK_PERMITTED	2
#define TCPOPT_SACK	5
#define	TCPOPT_TIMESTAMP	8
#define 	TCPOLEN_TIMESTAMP	10
#define		TCPOLEN_TSTAMP_APPA	(TCPOLEN_TIMESTAMP+2)

#define TCPOPT_TSTAMP_HDR	\
	(TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)



#define TCP_MSS	512
#define TCP_MAXWIN	65535
#define TCP_MAX_WINSHIFT	14

#define TCP_NODELAY	0x01
#define TCP_MAXSEG	0x02


#endif /* TCP_H_ */
