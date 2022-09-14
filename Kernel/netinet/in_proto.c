/*
 * in_proto.c
 *
 *  Created on: Oct 11, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/sys/socket.h>
#include <kernel/sys/protosw.h>
#include <kernel/sys/domain.h>
#include <kernel/mbuf.h>

#include <kernel/net/if.h>
#include <kernel/net/radix.h>
#include <kernel/net/route.h>

#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/netinet/ip_icmp.h>
#include <kernel/sys/netinet/ip_igmp.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/netinet/tcp.h>
#include <kernel/sys/netinet/tcp_fsm.h>
#include <kernel/sys/netinet/tcp_seq.h>
#include <kernel/sys/netinet/tcp_timer.h>
#include <kernel/sys/netinet/tcp_var.h>
#include <kernel/sys/netinet/tcpip.h>
#include <kernel/sys/netinet/udp.h>
#include <kernel/sys/netinet/udp_var.h>


extern struct domain inetdomain;
struct protosw inetsw[] = {
		//ip domain
		{
			.pr_type = 0, .pr_domain = &inetdomain, .pr_protocol = 0,
			.pr_flags = 0, .pr_input = NULL, .pr_output = ip_output,
			.pr_ctlinput = NULL, .pr_ctloutput = NULL, .pr_usrreq = NULL,
			.pr_init = ip_init, .pr_fasttimo = NULL, .pr_slowtimo = ip_slowtimo,
			.pr_drain = ip_drain, .pr_sysctl = NULL
		},
		//udp domain
		{
			.pr_type = SOCK_DGRAM, &inetdomain, .pr_protocol = IPPROTO_UDP,
			.pr_flags = PR_ATOMIC|PR_ADDR, .pr_input = udp_input, .pr_output = NULL,
			.pr_ctlinput = udp_ctlinput, .pr_ctloutput = ip_ctloutput, .pr_usrreq = udp_usrreq,
			.pr_init = udp_init, .pr_fasttimo = NULL, .pr_slowtimo = NULL,
			.pr_drain = NULL, .pr_sysctl = NULL
		},
		//tcp domain
		{
			.pr_type = SOCK_STREAM, &inetdomain, .pr_protocol = IPPROTO_TCP,
			.pr_flags = PR_CONNREQUIRED|PR_WANTRCVD, .pr_input = tcp_input, .pr_output = NULL,
			.pr_ctlinput = NULL, .pr_ctloutput = NULL, .pr_usrreq = tcp_usrreq,
			.pr_init = tcp_init, .pr_fasttimo = tcp_fasttimo, .pr_slowtimo = tcp_slowtimo,
			.pr_drain = tcp_drain, .pr_sysctl = NULL
		},
		//raw,
		{
			.pr_type = SOCK_RAW, &inetdomain, .pr_protocol = IPPROTO_RAW,
			.pr_flags = PR_ATOMIC|PR_ADDR, .pr_input = rip_input, .pr_output = rip_output,
			.pr_ctlinput = NULL, .pr_ctloutput = rip_ctloutput, .pr_usrreq = rip_usrreq,
			.pr_init = rip_init, .pr_fasttimo = NULL, .pr_slowtimo = NULL,
			.pr_drain = NULL, .pr_sysctl = NULL
		},
		//raw, icmp
		{
			.pr_type = SOCK_RAW, &inetdomain, .pr_protocol = IPPROTO_ICMP,
			.pr_flags = PR_ATOMIC|PR_ADDR, .pr_input = icmp_input, .pr_output = rip_output,
			.pr_ctlinput = NULL, .pr_ctloutput = rip_ctloutput, .pr_usrreq = rip_usrreq,
			.pr_init = NULL, .pr_fasttimo = NULL, .pr_slowtimo = NULL,
			.pr_drain = NULL, .pr_sysctl = NULL
		},
		//raw,igmp
		{
			.pr_type = SOCK_RAW, &inetdomain, .pr_protocol = IPPROTO_IGMP,
			.pr_flags = PR_ATOMIC|PR_ADDR, .pr_input = NULL, .pr_output = rip_output,
			.pr_ctlinput = NULL, .pr_ctloutput = rip_ctloutput, .pr_usrreq = rip_usrreq,
			.pr_init = igmp_init, .pr_fasttimo = igmp_fasttimo, .pr_slowtimo = NULL,
			.pr_drain = NULL, .pr_sysctl = NULL
		},

		//raw,  TODO:unimplenment
		{
			.pr_type = SOCK_RAW, &inetdomain, .pr_protocol = 0,
			.pr_flags = PR_ATOMIC|PR_ADDR, .pr_input = rip_input, .pr_output = rip_output,
			.pr_ctlinput = NULL, .pr_ctloutput = rip_ctloutput, .pr_usrreq = rip_usrreq,
			.pr_init = rip_init, .pr_fasttimo = NULL, .pr_slowtimo = NULL,
			.pr_drain = NULL, .pr_sysctl = NULL
		},

};

struct domain inetdomain = {
		.dom_family = AF_INET,
		.dom_name = "Internet",
		.dom_init = NULL,
		.dom_externalize = NULL,
		.dom_dispose = NULL,
		.dom_protosw = inetsw,
		.dom_protoswNPROTOSW = &inetsw[sizeof(inetsw)/sizeof(inetsw[0])],
		.dom_next = NULL,
		.dom_rtattach = rn_inithead,
		.dom_rtoffset = 32,
		.dom_maxrtkey = sizeof(struct sockaddr_in),
};











