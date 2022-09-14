/*
 * ip_igmp.c
 *
 *  Created on: Nov 21, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/mbuf.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/netinet/ip_igmp.h>

#include <string.h>

static int igmp_timers_are_running = 0;
static u32 igmp_all_hosts_group;
extern int in_cksum(struct mbuf *m, int len);
extern struct 	in_ifaddr *in_ifaddr;

struct igmpstat igmpstat;

static void
igmp_sendreport(struct in_multi *inm);

void
igmp_init()
{
	/*
	 * To avoid byte-swapping the same value over and over again.
	 */
	igmp_all_hosts_group = htonl(INADDR_ALLHOSTS_GROUP);
}

void
igmp_input(struct mbuf *m, int iphlen)
{
	struct igmp *igmp;
	struct ip *ip;
	int igmplen;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	int minlen;
	struct in_multi *inm;
	struct in_ifaddr *ia;
	struct in_multistep step;

	++igmpstat.igps_rcv_total;

	ip = mtod(m, struct ip *);
	igmplen = ip->ip_len;

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		++igmpstat.igps_rcv_tooshort;
		m_freem(m);
		return;
	}
	minlen = iphlen + IGMP_MINLEN;
	if ((m->m_flags & M_EXT || m->m_len < minlen) &&
	    (m = m_pullup(m, minlen)) == 0) {
		++igmpstat.igps_rcv_tooshort;
		return;
	}

	/*
	 * Validate checksum
	 */
	m->m_data += iphlen;
	m->m_len -= iphlen;
	igmp = mtod(m, struct igmp *);
	if (in_cksum(m, igmplen)) {
		++igmpstat.igps_rcv_badsum;
		m_freem(m);
		return;
	}
	m->m_data -= iphlen;
	m->m_len += iphlen;
	ip = mtod(m, struct ip *);

	switch (igmp->igmp_type) {

	case IGMP_HOST_MEMBERSHIP_QUERY:
		++igmpstat.igps_rcv_queries;

//		if (ifp == &loif)
//			break;

		if (ip->ip_dst.s_addr != igmp_all_hosts_group) {
			++igmpstat.igps_rcv_badqueries;
			m_freem(m);
			return;
		}

		/*
		 * Start the timers in all of our membership records for
		 * the interface on which the query arrived, except those
		 * that are already running and those that belong to the
		 * "all-hosts" group.
		 */
		IN_FIRST_MULTI(step, inm);
		while (inm != NULL) {
			if (inm->inm_ifp == ifp && inm->inm_timer == 0 &&
			    inm->inm_addr.s_addr != igmp_all_hosts_group) {
				inm->inm_timer =
				    IGMP_RANDOM_DELAY(inm->inm_addr);
				igmp_timers_are_running = 1;
			}
			IN_NEXT_MULTI(step, inm);
		}

		break;

	case IGMP_HOST_MEMBERSHIP_REPORT:
		++igmpstat.igps_rcv_reports;

//		if (ifp == &loif)
//			break;

		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr)) ||
		    igmp->igmp_group.s_addr != ip->ip_dst.s_addr) {
			++igmpstat.igps_rcv_badreports;
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0) {
			IFP_TO_IA(ifp, ia);
			if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);
		}

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN_LOOKUP_MULTI(igmp->igmp_group, ifp, inm);
		if (inm != NULL) {
			inm->inm_timer = 0;
			++igmpstat.igps_rcv_ourreports;
		}

		break;
	}

	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket.
	 */
	rip_input(m);
}

void
igmp_joingroup(struct in_multi *inm)
{

	if (inm->inm_addr.s_addr == igmp_all_hosts_group /*||
	    inm->inm_ifp == &loif*/)
		inm->inm_timer = 0;
	else {
		igmp_sendreport(inm);
		inm->inm_timer = IGMP_RANDOM_DELAY(inm->inm_addr);
		igmp_timers_are_running = 1;
	}

}

void
igmp_leavegroup(struct in_multi *inm)
{
	/*
	 * No action required on leaving a group.
	 */
}

void
igmp_fasttimo()
{
	struct in_multi *inm;
	struct in_multistep step;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!igmp_timers_are_running)
		return;

	igmp_timers_are_running = 0;
	IN_FIRST_MULTI(step, inm);
	while (inm != NULL) {
		if (inm->inm_timer == 0) {
			/* do nothing */
		} else if (--inm->inm_timer == 0) {
			igmp_sendreport(inm);
		} else {
			igmp_timers_are_running = 1;
		}
		IN_NEXT_MULTI(step, inm);
	}

}

static void
igmp_sendreport(struct in_multi *inm)
{
	struct mbuf *m;
	struct igmp *igmp;
	struct ip *ip;
	struct ip_moptions *imo;
	struct ip_moptions simo;

	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
	/*
	 * Assume max_linkhdr + sizeof(struct ip) + IGMP_MINLEN
	 * is smaller than mbuf size returned by MGETHDR.
	 */
	m->m_data += max_linkhdr;
	m->m_len = sizeof(struct ip) + IGMP_MINLEN;
	m->m_pkthdr.len = sizeof(struct ip) + IGMP_MINLEN;

	ip = mtod(m, struct ip *);
	ip->ip_tos = 0;
	ip->ip_len = sizeof(struct ip) + IGMP_MINLEN;
	ip->ip_off = 0;
	ip->ip_p = IPPROTO_IGMP;
	ip->ip_src.s_addr = INADDR_ANY;
	ip->ip_dst = inm->inm_addr;

	m->m_data += sizeof(struct ip);
	m->m_len -= sizeof(struct ip);
	igmp = mtod(m, struct igmp *);
	igmp->igmp_type = IGMP_HOST_MEMBERSHIP_REPORT;
	igmp->igmp_code = 0;
	igmp->igmp_group = inm->inm_addr;
	igmp->igmp_cksum = 0;
	igmp->igmp_cksum = in_cksum(m, IGMP_MINLEN);
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);

	imo = &simo;
	bzero((caddr_t)imo, sizeof(*imo));
	imo->imo_multicast_ifp = inm->inm_ifp;
	imo->imo_multicast_ttl = 1;
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing demon can hear it.
	 */
#ifdef MROUTING
    {
	extern struct socket *ip_mrouter;
	imo->imo_multicast_loop = (ip_mrouter != NULL);
    }
#endif
	ip_output(m, NULL, NULL, 0, imo);

	++igmpstat.igps_snd_reports;
}
