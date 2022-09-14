/*
 * ip_output.c
 *
 *  Created on: Jul 12, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <kernel/net/route.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/ip_val.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/protosw.h>


extern int max_linkhdr;
extern struct 	ipstat	ipstat;
extern struct 	in_ifaddr *in_ifaddr;
extern int in_cksum(struct mbuf *m, int len);

u16 ip_id;
struct ipq ipq;

extern int
in_delmulti(struct in_multi *inm);
int ip_pcboptions(struct mbuf **pcbopt, struct mbuf *m)
{
	int cnt, optlen;
	u8 *cp;
	u8	opt;

	if (*pcbopt){
		m_free(*pcbopt);
	}

	*pcbopt = NULL;
	if (m == NULL || m->m_len == 0){
		if (m)
			m_free(m);
		return 0;
	}

	if (m->m_len % sizeof(long))
		goto bad;

	if (m->m_data + m->m_len + sizeof(struct in_addr) >= &m->m_dat[MLEN])
		goto bad;

	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);

	cp = mtod(m, u8 *) +sizeof(struct in_addr);
	memmove((void *)cp, (void *)mtod(m, caddr_t), cnt);
	memset((void *)mtod(m, caddr_t), 0, sizeof(struct in_addr));


	for (; cnt > 0; cnt -= optlen, cp += optlen){
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if(optlen <= IPOPT_OLEN || optlen > cnt)
				goto bad;
		}

		switch (opt){
		default:
			break;
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifiles route as:
			 * ->A->B->C->D
			 *
			 * A is the first hop destination,which doesn't
			 * appear in actual ip option,but is stored
			 * before the options
			 *
			 */
			//option length is invalid
			if (optlen < IPOPT_MINOFF -1 + sizeof(struct in_addr))
				goto bad;

			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			memcpy((void *)mtod(m, caddr_t), (void *)&cp[IPOPT_OFFSET+1], sizeof(struct in_addr));

			memmove((void *)&cp[IPOPT_OFFSET+1], (void *)(&cp[IPOPT_OFFSET+1]+sizeof(struct in_addr)),
					cnt + sizeof(struct in_addr));
			break;
		}
	}

	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;


	*pcbopt = m;
	return 0;
bad:
	m_free(m);
	return EINVAL;
}

int ip_optcopy(struct ip *ip, struct ip *jp)
{
	u8	*cp, *dp;
	int opt, optlen, cnt;

	cp = (u8 *)(ip+1);
	dp = (u8 *)(jp+1);
	cnt = (ip->ip_hl << 2) - sizeof(struct ip);
	for (; cnt>0; cnt -= optlen, cp += optlen){
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP){
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		}else
			optlen = cp[IPOPT_OLEN];

		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)){
			memcpy((void *)dp, (void *)cp, (unsigned)optlen);
			cp += optlen;
		}

	}

	for (optlen = dp - (u8 *)(jp+1); optlen & 0x03; optlen++)
		*dp++ = IPOPT_EOL;

	return optlen;
}



static struct mbuf *
ip_insert_options(struct mbuf *m, struct mbuf *opt, int *phlen)
{
	struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + (u16)ip->ip_len > IP_MAXPACKET)
		return m;

	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if ((m->m_flags & M_EXT) || m->m_data - optlen < m->m_pktdat){
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == NULL){
			return m;
		}

		n->m_pkthdr.len = m->m_pkthdr.len + optlen;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		memcpy((void *)mtod(m, caddr_t), (void *)ip, sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
		memcpy((void *)mtod(m, caddr_t), (void *)ip, sizeof(struct ip));
	}

	ip = mtod(m, struct ip *);
	memcpy((void *)(ip+1), (void *)p->ipopt_list, optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len += optlen;
	return m;
}


/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(struct ip_moptions *imo)
{
	int i;
	LOG_INFO("in");
	if (imo != NULL) {
		for (i = 0; i < imo->imo_num_memberships; ++i)
			in_delmulti(imo->imo_membership[i]);
		kfree(imo);
	}
	LOG_INFO("end");
}


int ip_output(struct mbuf *m0, struct mbuf *opt,
		struct route *ro, int flags,
		struct ip_moptions *imo)
{
	struct ip *ip, *mhip;
	struct ifnet *ifp;
	struct mbuf *m = m0;
	int hlen = sizeof(struct ip);
	int len, off, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct in_ifaddr *ia;

	LOG_INFO("in, ro:%x", (u32)ro);
	if (opt){
		m = ip_insert_options(m, opt, &len);
		hlen = len;
	}

	ip = mtod(m, struct ip *);

	/*
	 * fill in ip header
	 */

	if ((flags & (IP_FORWARDING | IP_RAWOUTPUT)) == 0){
		ip->ip_v = IPVERSION;
		ip->ip_off &= IP_DF;
		ip->ip_id = htons(ip_id);
		ip_id++;
		ip->ip_hl = hlen >> 2;
		ipstat.ips_localout++;
	} else {
		hlen = ip->ip_hl << 2;
	}


	if (ro == NULL){
		ro = &iproute;
		memset((void *)ro, 0, sizeof(*ro));
	}
	//get a route entry
	dst = (struct sockaddr_in *)&ro->ro_dst;
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
			dst->sin_addr.s_addr != ip->ip_dst.s_addr)){
		RTFREE(ro->ro_rt);
		ro->ro_rt  = NULL;
	}
	LOG_INFO("dst:%x, ro_rt:%x", ((struct sockaddr_in *)dst)->sin_addr.s_addr, ro->ro_rt);
	if (ro->ro_rt == NULL){
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
	}

#define ifatoia(ifa)	((struct in_ifaddr *)(ifa))
#define sintosa(sin)	((struct sockaddr *)(sin))

	if (flags & IP_ROUTETOIF){
		if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == NULL &&
				(ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == NULL	){
			ipstat.ips_noroute++;
			error = -ENETUNREACH;
			goto bad;
		}

		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
	} else {
		if (ro->ro_rt == NULL)
			rtalloc(ro);
		if (ro->ro_rt == NULL){
			ipstat.ips_noroute++;
			error = -EHOSTUNREACH;
			goto bad;
		}

		ia = ifatoia(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;

		ro->ro_rt->rt_use++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;

		LOG_INFO("ip if unit:%d, dst:%x", ifp->if_unit, dst->sin_addr.s_addr);
	}

	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))){
		struct in_multi *inm;
		//extern struct ifnet loif;
		m->m_flags |= M_MCAST;
		dst = (struct sockaddr_in *)&ro->ro_dst;
		if (imo != NULL){
			ip->ip_ttl = imo->imo_multicast_ttl;
			if (imo->imo_multicast_ifp != NULL)
				ifp = imo->imo_multicast_ifp;
		}else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;


		if ((ifp->if_flags & IFF_MULTICAST) == 0){
			ipstat.ips_noroute++;
			error = -ENETUNREACH;
			goto bad;
		}


		if (ip->ip_src.s_addr == INADDR_ANY){
			struct in_ifaddr *ia;
			for (ia = in_ifaddr; ia; ia = ia->ia_next){
				if (ia->ia_ifp == ifp){
					ip->ip_src = IA_SIN(ia)->sin_addr;
					break;
				}
			}
		}


		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm != NULL &&
				(imo == NULL || imo->imo_multicast_loop)){
			//TODO:loopback
			//ip_mloopback(ifp, m, dst);
			LOG_INFO("muilti loopback");
		} else {
			//TODO:multicast route
			//multicast routing not supported yet
			//if the host was configured as a multicast router
			LOG_INFO("multicast routing");
		}

		if(ip->ip_ttl == 0 /* || ifp == &loif */){
			m_freem(m);
			goto done;
		}
		goto sendit;

	}



	if (ip->ip_src.s_addr == INADDR_ANY)
		ip->ip_src = IA_SIN(ia)->sin_addr;

	LOG_INFO("ip src:%x", ip->ip_src);
	if (in_broadcast(dst->sin_addr, ifp)){
		LOG_INFO("broadcast addr");
		if ((ifp->if_flags & IFF_BROADCAST) == 0){
			error = -EADDRNOTAVAIL;
			goto bad;
		}

		if ((flags & IP_ALLOWBROADCAST) == 0){
			error = -EACCES;
			goto bad;
		}

		if ((u16)ip->ip_len > ifp->if_mtu){
			error = -EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else
		m->m_flags &= ~M_BCAST;

sendit:
	if ((u16)ip->ip_len <= ifp->if_mtu){
		ip->ip_len = htons((u16)ip->ip_len);
		ip->ip_off = htons((u16)ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst, ro->ro_rt);
		goto done;
	}

	/*
	 * fragmentation
	 */
	LOG_INFO("ip frag,len:%d, mtu:%d", ip->ip_len, ifp->if_mtu);
	if (ip->ip_off & IP_DF){
		error = -EMSGSIZE;
		ipstat.ips_cantfrag++;
		goto bad;
	}

	len = (ifp->if_mtu - hlen) & ~7;
	if (len < 8){
		error = -EMSGSIZE;
		goto bad;
	}

	do {
		int mhlen, firstlen = len;
		struct mbuf **mnext = &m->m_nextpkt;
		m0 = m;
		mhlen = sizeof(struct ip);
		for (off = hlen + len; off < (u16)ip->ip_len; off += len){
			MGETHDR(m, M_DONTWAIT, MT_HEADER);
			if (m == NULL){
				error = -ENOBUFS;
				ipstat.ips_odropped++;
				goto sendorfree;
			}

			m->m_data += max_linkhdr;
			mhip = mtod(m, struct ip *);
			*mhip = *ip;

			if (hlen > sizeof(struct ip)){
				mhlen = ip_optcopy(ip, mhip) + sizeof(struct ip);
				mhip->ip_hl = mhlen >> 2;
			}

			m->m_len = mhlen;
			mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
			if (ip->ip_off & IP_MF)
				mhip->ip_off |= IP_MF;
			if (off + len >= (u16)ip->ip_len)
				len = (u16)ip->ip_len - off;	//this is the last fragment
			else
				mhip->ip_off |= IP_MF;			//for other fragments, just set IP_MF flag
			mhip->ip_len = htons((u16)(len + mhlen));
			m->m_next = m_copy(m0, off, len);		//copy data to new mbuf
			if (m->m_next == 0){
				m_free(m);
				error = ENOBUFS;
				ipstat.ips_odropped++;
				goto sendorfree;
			}

			m->m_pkthdr.len = mhlen + len;
			m->m_pkthdr.rcvif = NULL;
			mhip->ip_off = htons((u16)mhip->ip_off);
			mhip->ip_sum = 0;
			mhip->ip_sum = in_cksum(m, mhlen);
			*mnext = m;
			mnext = &m->m_nextpkt;
			ipstat.ips_ofragments++;
		}

		m = m0;
		m_adj(m, hlen + firstlen - (u16)ip->ip_len);
		m->m_pkthdr.len = hlen + firstlen;
		ip->ip_len = htons((u16)m->m_pkthdr.len);
		ip->ip_off = htons((u16)(ip->ip_off | IP_MF));
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
sendorfree:
		for(m=m0; m; m=m0){
			m0 = m->m_nextpkt;
			m->m_nextpkt = 0;
			if (error == 0)
				error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst, ro->ro_rt);
			else
				m_free(m);
		}

		if(error == 0)
			ipstat.ips_fragmented++;
	} while (0);

done:

	if ((ro == &iproute) && ((flags & IP_ROUTETOIF) == 0) && ro->ro_rt){
		RTFREE(ro->ro_rt);
	}
	LOG_INFO("out.error:%d", -error);
	return (error);
bad:
	m_freem(m0);
	goto done;
}

int
ip_pcbopts(struct mbuf **pcbopt, struct mbuf *m)
{
	int cnt, optlen;
	u8 *cp;
	u8 opt;

	/* turn off any old options */
	if (*pcbopt)
		(void)m_free(*pcbopt);
	*pcbopt = 0;
	if (m == (struct mbuf *)0 || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m)
			(void)m_free(m);
		return (0);
	}

#ifndef	vax
	if (m->m_len % sizeof(long))
		goto bad;
#endif
	/*
	 * IP first-hop destination address will be stored before
	 * actual options; move other options back
	 * and clear it when none present.
	 */
	if (m->m_data + m->m_len + sizeof(struct in_addr) >= &m->m_dat[MLEN])
		goto bad;
	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);
	cp = mtod(m, u8 *) + sizeof(struct in_addr);
	memmove((void *)mtod(m, caddr_t), (void *)cp, (unsigned)cnt);
	bzero(mtod(m, caddr_t), sizeof(struct in_addr));

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= IPOPT_OLEN || optlen > cnt)
				goto bad;
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifies route as:
			 *	->A->B->C->D
			 * D must be our final destination (but we can't
			 * check that since we may not have connected yet).
			 * A is first hop destination, which doesn't appear in
			 * actual IP option, but is stored before the options.
			 */
			if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
				goto bad;
			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			/*
			 * Move first hop before start of options.
			 */
			bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
			    sizeof(struct in_addr));
			/*
			 * Then copy rest of options back
			 * to close up the deleted entry.
			 */
			memmove((void *)(&cp[IPOPT_OFFSET+1] +
			    sizeof(struct in_addr)),
			    (void *)&cp[IPOPT_OFFSET+1],
			    (unsigned)cnt + sizeof(struct in_addr));
			break;
		}
	}
	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;
	*pcbopt = m;
	return (0);

bad:
	(void)m_free(m);
	return (-EINVAL);
}

int
ip_getmoptions(int optname, struct ip_moptions *imo, struct mbuf **mp)
{
	u8 *ttl;
	u8 *loop;
	struct in_addr *addr;
	struct in_ifaddr *ia;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {
	case IP_MULTICAST_IF:
		addr = mtod(*mp, struct in_addr *);
		(*mp)->m_len = sizeof(struct in_addr);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			addr->s_addr = INADDR_ANY;
		else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			addr->s_addr = (ia == NULL) ? INADDR_ANY
					: IA_SIN(ia)->sin_addr.s_addr;
		}
		return (0);

	case IP_MULTICAST_TTL:
		ttl = mtod(*mp, u8 *);
		(*mp)->m_len = 1;
		*ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_multicast_ttl;
		return (0);

	case IP_MULTICAST_LOOP:
		loop = mtod(*mp, u8 *);
		(*mp)->m_len = 1;
		*loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_multicast_loop;
		return (0);

	default:
		return (-EOPNOTSUPP);
	}
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
ip_setmoptions(int optname, struct ip_moptions **imop, struct mbuf *m)
{
	int error = 0;
	u8 loop;
	int i;
	struct in_addr addr;
	struct ip_mreq *mreq;
	struct ifnet *ifp;
	struct ip_moptions *imo = *imop;
	struct route ro;
	struct sockaddr_in *dst;

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = (struct ip_moptions*)kmalloc(sizeof(*imo));
		if (imo == NULL)
			return (-ENOBUFS);
		*imop = imo;
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
	}

	switch (optname) {

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(struct in_addr)) {
			error = -EINVAL;
			break;
		}
		addr = *(mtod(m, struct in_addr *));
		/*
		 * INADDR_ANY is used to remove a previous selection.
		 * When no interface is selected, a default one is
		 * chosen every time a multicast packet is sent.
		 */
		if (addr.s_addr == INADDR_ANY) {
			imo->imo_multicast_ifp = NULL;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 */
		INADDR_TO_IFP(addr, ifp);
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = -EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = -EINVAL;
			break;
		}
		imo->imo_multicast_ttl = *(mtod(m, u8 *));
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != 1 ||
		   (loop = *(mtod(m, u8 *))) > 1) {
			error = -EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = -EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
			error = -EINVAL;
			break;
		}
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY) {
			ro.ro_rt = NULL;
			dst = (struct sockaddr_in *)&ro.ro_dst;
			dst->sin_len = sizeof(*dst);
			dst->sin_family = AF_INET;
			dst->sin_addr = mreq->imr_multiaddr;
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
				error = -EADDRNOTAVAIL;
				break;
			}
			ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		}
		else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
		}
		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = -EADDRNOTAVAIL;
			break;
		}
		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    imo->imo_membership[i]->inm_addr.s_addr
						== mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = -EADDRINUSE;
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = -ETOOMANYREFS;
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
			error = -ENOBUFS;
			break;
		}
		++imo->imo_num_memberships;
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = -EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
			error = -EINVAL;
			break;
		}
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY)
			ifp = NULL;
		else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
			if (ifp == NULL) {
				error = -EADDRNOTAVAIL;
				break;
			}
		}
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     imo->imo_membership[i]->inm_addr.s_addr ==
			     mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = -EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		in_delmulti(imo->imo_membership[i]);
		/*
		 * Remove the gap in the membership array.
		 */
		for (++i; i < imo->imo_num_memberships; ++i)
			imo->imo_membership[i-1] = imo->imo_membership[i];
		--imo->imo_num_memberships;
		break;

	default:
		error = -EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_num_memberships == 0) {
		kfree(*imop);
		*imop = NULL;
	}

	return (error);
}



/*
 * IP socket option processing.
 */
int
ip_ctloutput(int op, struct socket *so, int level, int optname, struct mbuf **mp)
{
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *m = *mp;
	int optval;
	int error = 0;

	if (level != IPPROTO_IP) {
		error = -EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
	} else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
			return (ip_pcbopts(&inp->inp_options, m));
		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
			if (m->m_len != sizeof(int))
				error = -EINVAL;
			else {
				optval = *mtod(m, int *);
				switch (optname) {

				case IP_TOS:
					inp->inp_ip.ip_tos = optval;
					break;

				case IP_TTL:
					inp->inp_ip.ip_ttl = optval;
					break;
#define	OPTSET(bit) \
	if (optval) \
		inp->inp_flags |= bit; \
	else \
		inp->inp_flags &= ~bit;

				case IP_RECVOPTS:
					OPTSET(INP_RECVOPTS);
					break;

				case IP_RECVRETOPTS:
					OPTSET(INP_RECVRETOPTS);
					break;

				case IP_RECVDSTADDR:
					OPTSET(INP_RECVDSTADDR);
					break;
				}
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(optname, &inp->inp_moptions, m);
			break;

		default:
			error = -ENOPROTOOPT;
			break;
		}
		if (m)
			(void)m_free(m);
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			if (inp->inp_options) {
				m->m_len = inp->inp_options->m_len;
				bcopy(mtod(inp->inp_options, caddr_t),
				    mtod(m, caddr_t), (unsigned)m->m_len);
			} else
				m->m_len = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			switch (optname) {

			case IP_TOS:
				optval = inp->inp_ip.ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip.ip_ttl;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;
			}
			*mtod(m, int *) = optval;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(optname, inp->inp_moptions, mp);
			break;

		default:
			error = -ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}










