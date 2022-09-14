/*
 * dhcp.h
 *
 *  Created on: Oct 21, 2013
 *      Author: root
 */

#ifndef SHAUN_DHCP_H_
#define SHAUN_DHCP_H_


#include <kernel/types.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/udp.h>

struct dhcp {
    u8		dp_op;		/* packet opcode type */
    u8		dp_htype;	/* hardware addr type */
    u8		dp_hlen;	/* hardware addr length */
    u8		dp_hops;	/* gateway hops */
    u32		dp_xid;		/* transaction ID */
    u16		dp_secs;	/* seconds since boot began */
    u16		dp_flags;	/* flags */
    struct in_addr	dp_ciaddr;	/* client IP address */
    struct in_addr	dp_yiaddr;	/* 'your' IP address */
    struct in_addr	dp_siaddr;	/* server IP address */
    struct in_addr	dp_giaddr;	/* gateway IP address */
    u8		dp_chaddr[16];	/* client hardware address */
    u8		dp_sname[64];	/* server host name */
    u8		dp_file[128];	/* boot file name */
    u8		dp_options[0];	/* variable-length options field */
};

struct dhcp_packet {
    struct ip 		ip;
    struct udphdr 	udp;
    struct dhcp 	dhcp;
};

#define DHCP_OPTIONS_MIN	312
#define DHCP_PACKET_MIN		(sizeof(struct dhcp_packet) + DHCP_OPTIONS_MIN)
#define DHCP_PAYLOAD_MIN	(sizeof(struct dhcp) + DHCP_OPTIONS_MIN)

/* dhcp message types */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

enum {
    dhcp_msgtype_none_e		= 0,
    dhcp_msgtype_discover_e 	= DHCPDISCOVER,
    dhcp_msgtype_offer_e	= DHCPOFFER,
    dhcp_msgtype_request_e	= DHCPREQUEST,
    dhcp_msgtype_decline_e	= DHCPDECLINE,
    dhcp_msgtype_ack_e		= DHCPACK,
    dhcp_msgtype_nak_e		= DHCPNAK,
    dhcp_msgtype_release_e	= DHCPRELEASE,
    dhcp_msgtype_inform_e	= DHCPINFORM,
};

typedef u8 dhcp_msgtype_t;

typedef u32			dhcp_time_secs_t; /* absolute time */
typedef u32			dhcp_lease_t;     /* relative time */
#define dhcp_time_hton		htonl
#define dhcp_time_ntoh		ntohl
#define dhcp_lease_hton		htonl
#define dhcp_lease_ntoh		ntohl

#define DHCP_INFINITE_LEASE	((dhcp_lease_t)-1)
#define DHCP_INFINITE_TIME	((dhcp_time_secs_t)-1)

#define DHCP_FLAGS_BROADCAST	((u16)0x0001)

extern int dhcp_get_ip();

#endif /* SHAUN_DHCP_H_ */
