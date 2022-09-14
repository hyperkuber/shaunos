/*
 * ip_input.c
 *
 *  Created on: Jun 27, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/mbuf.h>
#include <kernel/malloc.h>
#include <kernel/net/if.h>
#include <kernel/net/route.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/ip_icmp.h>
#include <string.h>
#include <kernel/sys/protosw.h>
#define	IPFORWARDING	0	/* forward IP packets not for us */
#define	IPSENDREDIRECTS	1

int	ipforwarding = IPFORWARDING;
int	ipsendredirects = IPSENDREDIRECTS;
int	ip_defttl = IPDEFTTL;

struct	ifqueue ipintrq;
struct 	ipstat	ipstat;
struct 	in_ifaddr *in_ifaddr;
struct 	route	ip_forward_rt;
int	ip_nhops = 0;	//for previous source route option


extern	struct domain inetdomain;
extern	struct protosw inetsw[];
u8	ip_protox[IPPROTO_MAX];
int	ipqmaxlen = IFQ_MAXLEN;


static struct ip_srcrt {
	struct in_addr dst;
	char nop;
	char srcopt[IPOPT_OFFSET + 1];
	struct in_addr route[(MAX_IPOPTLEN -3) / sizeof(struct in_addr)];
}ip_srcrt;

u8 inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };

u32	*ip_ifmatrix;
extern int if_index;
extern u16 ip_id;
extern unsigned short
in_cksum(struct mbuf *m,int len);

void
ip_stripoptions(struct mbuf *m, struct mbuf *mopt)
{
	int i;
	struct ip *ip = mtod(m, struct ip *);
	caddr_t opts;
	int olen;

	olen = (ip->ip_hl<<2) - sizeof (struct ip);
	opts = (caddr_t)(ip + 1);
	i = m->m_len - (sizeof (struct ip) + olen);
	bcopy(opts + olen, opts, (unsigned)i);
	m->m_len -= olen;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len -= olen;
	ip->ip_hl = sizeof(struct ip) >> 2;
}

void ip_forward(struct mbuf *m, int srcrt)
{
	struct ip *ip = mtod(m, struct ip *);
	struct sockaddr_in *sin;
	struct rtentry *rt;

	int error, type = 0, code;
	struct mbuf *mcopy;
	u32	dest;
	struct ifnet *destifp;

	dest = 0;

	LOG_INFO("in");

	if ((m->m_flags & (M_BCAST | M_MCAST)) ||
			in_canforward(ip->ip_dst) == 0){
		ipstat.ips_cantforward++;
		LOG_WARN("do not forward broadcast packet");
		m_freem(m);
		return;
	}

	//htons(ip->ip_id);
	if (ip->ip_ttl <= IPTTLDEC){
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
		return;
	}

	ip->ip_ttl -= IPTTLDEC;
	/*
	 * ok, let's figure out the next hop,
	 * we cache the last routed entry in
	 * ip_forward_rt to save performance,
	 * [Jain & Routhier 1986] & [Mongul 1991]
	 */
	sin = (struct sockaddr_in *)&ip_forward_rt.ro_dst;

	if ((rt = ip_forward_rt.ro_rt) == NULL ||
			ip->ip_dst.s_addr != sin->sin_addr.s_addr){
		//not match, free last cache
		if (ip_forward_rt.ro_rt){
			RTFREE(ip_forward_rt.ro_rt);
			ip_forward_rt.ro_rt = NULL;
		}
		//and get a new entry
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;

		rtalloc(&ip_forward_rt);

		if(ip_forward_rt.ro_rt == NULL){
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
			return;
		}

		rt = ip_forward_rt.ro_rt;

	}
	//save at most 64 bytes to generate icmp message to the src
	//TODO:ip_len is network byte order, change it to host byte order
	mcopy = m_copy(m, 0, min((int)ip->ip_len, 64));
	//ip_ifmatrix record the number of routed packets
	ip_ifmatrix[rt->rt_ifp->if_index + if_index * m->m_pkthdr.rcvif->if_index]++;


	/*
	 * Next, we should figure out whether redirect is needed or not,
	 * the policy is:
	 * 1) the packet must be in the same interface that it came in on,
	 * 2) the dest route must not be created or modified by an icmp redirect
	 * 3) we must be authorized, ipsendredirects = 1;
	 * 4) for source routed(ip options), we do not send icmp redirect
	 * 5) we do not send redirect if dest route is default (0.0.0.0)
	 */

#define satosin(sa)	((struct sockaddr_in *)(sa))
	if (rt->rt_ifp == m->m_pkthdr.rcvif &&
			((rt->rt_flags & (RTF_DYNAMIC | RTF_MODIFIED)) == 0) &&
			satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
			ipsendredirects &&
			!srcrt){
#define RTA(rt)	((struct in_ifaddr *)(rt->rt_ifa))

		u32	src = ntohl(ip->ip_src.s_addr);
		if (RTA(rt) &&
				/*
				 * src and next hop in the same net
				 */
				(src & RTA(rt)->ia_subnetmask) == RTA(rt)->ia_subnet){
			if (rt->rt_flags & RTF_GATEWAY)
				dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
			else
				dest = ip->ip_dst.s_addr;

			type = ICMP_REDIRECT;
			code = ICMP_REDIRECT_HOST;
			LOG_INFO("redirect to %x", dest);
		}

	}

	error = ip_output(m, (struct mbuf *)0, &ip_forward_rt,
			IP_FORWARDING | IP_ALLOWBROADCAST, 0);

	if (error)
		ipstat.ips_forward++;
	else {
		ipstat.ips_forward++;
		if (type)
			ipstat.ips_redirectsent++;
		else {
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}

	if (mcopy == NULL)
		return;
	destifp = NULL;
	switch (error){
	case 0:	//forwarded, but need redirect
		break;
	case ENETUNREACH:
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;
	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;
		if (ip_forward_rt.ro_rt)
			destifp = ip_forward_rt.ro_rt->rt_ifp;
		ipstat.ips_cantfrag++;
		break;
	case ENOBUFS:
		type = ICMP_SOURCEQUENCH;
		code = 0;
		break;
	}

	icmp_error(mcopy, type, code, dest, destifp);
}



struct in_ifaddr *
ip_rtaddr(struct in_addr dst)
{
	struct sockaddr_in *sin;
	sin = (struct sockaddr_in *)&ip_forward_rt.ro_dst;

	if (ip_forward_rt.ro_rt == NULL || dst.s_addr != sin->sin_addr.s_addr){
		if (ip_forward_rt.ro_rt){
			RTFREE(ip_forward_rt.ro_rt);
			ip_forward_rt.ro_rt = NULL;
		}

		sin->sin_addr = dst;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);

		rtalloc(&ip_forward_rt);
	}

	if (ip_forward_rt.ro_rt == NULL)
		return NULL;
	return ((struct in_ifaddr *)ip_forward_rt.ro_rt->rt_ifa);

}

void save_rte(u8 *option, struct in_addr dst)
{
	unsigned olen;
	olen = option[IPOPT_OLEN];
	if (olen > sizeof(ip_srcrt) - (1+sizeof(dst)))
		return;

	memcpy((void *)ip_srcrt.srcopt, (void *)option, olen);
	ip_nhops = (olen - IPOPT_OFFSET -1) / sizeof(struct in_addr);
	ip_srcrt.dst = dst;
}


/*
 * process ip options in ip header,
 * usually forward packet,
 * return 1 if packet was forwarded,
 * else we need to do more work
 */
int ip_do_options(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	u8 *cp;
	struct ip_timestamp *ipt;
	struct in_ifaddr *ia;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr *sin, dst;
	n_time ntime;

	dst = ip->ip_dst;
	cp = (u8 *)(ip + 1);
	cnt  = (ip->ip_hl << 2) - sizeof( struct ip );

	for (; cnt > 0; cnt -= optlen, cp += optlen){
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {

			if (cnt < IPOPT_OLEN + sizeof(*cp)){
				code = &cp[IPOPT_OLEN] - (u8 *)ip;
				goto bad;
			}

			optlen = cp[IPOPT_OLEN];
			if (optlen <= IPOPT_OLEN + sizeof(*cp) || optlen > cnt){
				code = &cp[IPOPT_OLEN] - (u8 *)ip;
				goto bad;
			}
		}

#define SA	struct sockaddr *
#define INA	struct in_ifaddr *

		switch (opt){
		default :
			break;
		case IPOPT_RR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF){
				code = &cp[IPOPT_OFFSET] - (u8 *)ip;
				goto bad;
			}

			off--;
			if (off > optlen - sizeof(struct in_addr))
				break;

			memcpy((void *)&ipaddr.sin_addr, (void *)&ip->ip_dst, sizeof(ipaddr.sin_addr));
			if ((ia = (INA)ifa_ifwithaddr((SA)& ipaddr)) == NULL &&
					(ia = ip_rtaddr(ipaddr.sin_addr)) == NULL){
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			//copy address to ip header options
			memcpy((void *)(cp + off), (void *)&(IA_SIN(ia)->sin_addr), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF){
				code = &cp[IPOPT_OFFSET] - (u8 *)ip;
				goto bad;
			}

			ipaddr.sin_addr = ip->ip_dst;
			ia = (INA)ifa_ifwithaddr((SA)&ipaddr);
			if (ia == NULL){
				if (opt == IPOPT_SSRR){
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				//loose routing
				break;
			}
			off--;
			//no room for ip address in ip optinos area
			if (off > optlen - sizeof(struct in_addr)){
				save_rte(cp, ip->ip_src);
				break;
			}
			//save ip address
			memcpy((void *)&ipaddr.sin_addr, (void *)(cp+off), sizeof(ipaddr.sin_addr));
			if (opt == IPOPT_SSRR){
				//get an interface that match the dst ip address or dst net address
				//for strict source route record
				if ((ia = (INA)ifa_ifwithdstaddr((SA)&ipaddr)) == NULL)
					ia = (INA)ifa_ifwithnet((SA)&ipaddr);
			} else	//for loose source route record, lookup route table, to get next hop interface
				ia = ip_rtaddr(ipaddr.sin_addr);
			//we havn't found a proper interface
			if (ia == NULL){
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			//set dest address to the entry specified in  options
			ip->ip_dst = ipaddr.sin_addr;
			//copy outgoing interface address into options area
			memcpy((void *)(cp+off), (void *)&((IA_SIN(ia)->sin_addr)), sizeof(struct in_addr));
			//update offset
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			//if the dest address is not a multicast address, we should forward it , not return to ipintr
			forward = !IN_MULTICAST(ntohl(ip->ip_dst.s_addr));
			break;
		case IPOPT_TS:	//time stamp
			code = cp - (u8 *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_len < 5)
				goto bad;
			if (ipt->ipt_ptr > ipt->ipt_len - sizeof(long)){
				if (++ipt->ipt_oflw == 0)
					goto bad;
				break;
			}

			sin = (struct in_addr *)(cp + ipt->ipt_ptr -1);
			switch(ipt->ipt_flg){
			case IPOPT_TS_TSONLY:
				break;
			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr + sizeof(n_time) + sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				ipaddr.sin_addr = dst;
				ia = (INA)ifaof_ifpforaddr((SA)&ipaddr, m->m_pkthdr.rcvif);
				if (ia == NULL)
					continue;

				memcpy((void *)sin, (void *)&IA_SIN(ia)->sin_addr, sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;
			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr + sizeof(n_time) + sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;

				memcpy((void *)&ipaddr.sin_addr, (void *)sin, sizeof(struct in_addr));
				if (ifa_ifwithaddr((SA)&ipaddr) == NULL)
					continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				goto bad;
			}
			ntime = iptime();
			memcpy((void *)(cp + ipt->ipt_ptr -1), (void *)&ntime, sizeof(ntime));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}

	if (forward){
		ip_forward(m, 1);
		return 1;
	}

	return 0;
bad:
	ip->ip_len -= ip->ip_hl << 2;
	icmp_error(m, type, code, 0, 0);
	ipstat.ips_badoptions++;
	return 1;
}





struct mbuf *
ip_srcroute()
{
	struct in_addr *p, *q;
	struct mbuf *m;

	if (ip_nhops == 0)
		return NULL;
	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == NULL)
		return NULL;

#define OPTSIZ (sizeof(ip_srcrt.nop) + sizeof(ip_srcrt.srcopt))
	m->m_len = ip_nhops * sizeof(struct in_addr) + sizeof(struct in_addr) + OPTSIZ;

	p = &ip_srcrt.route[ip_nhops -1];
	*(mtod(m, struct in_addr *)) = *p--;

	ip_srcrt.nop = IPOPT_NOP;
	ip_srcrt.srcopt[IPOPT_OFFSET] = IPOPT_MINOFF;
	memcpy((void *)(mtod(m, caddr_t) + sizeof(struct in_addr)), (void *)&ip_srcrt.nop, OPTSIZ);
	q = (struct in_addr *)(mtod(m, caddr_t) + sizeof(struct in_addr) + OPTSIZ);

	while (p >= ip_srcrt.route){
		*q++ = *p--;
	}
	*q = ip_srcrt.dst;
	return m;
}


/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
ip_freef(struct ipq *fp)
{
	struct ipqent *q, *p;

	for (q = fp->ipq_fragq.first; q != NULL; q = p) {
		p = q->next;
		m_freem(q->ipqe_m);
		if (q->next != NULL)
			q->next->prev = q->prev;
		q->prev->next = q->next;
		kfree((void *)q);
	}
	if (fp->next != NULL)
		fp->next->prev = fp->prev;
	fp->prev->next = fp->next;
	kfree((void *)fp);
}


struct mbuf *
ip_reass(struct ipqent* ipqe, struct ipq *fp)
{
	struct mbuf *m = ipqe->ipqe_m;
	//struct ipasfrag *q;
	struct ipqent *nq, *p, *q;
	struct ip *ip;
	struct mbuf *t;
	int hlen = ipqe->ipqe_ip->ip_hl << 2;
	int i, next;

	m->m_data += hlen;
	m->m_len -= hlen;

	if (fp == NULL){
		if ((fp = (struct ipq *)kmalloc(sizeof(*fp))) == NULL)
			goto dropfrag;
		//insert head of ipq
		if ((fp->next = ipq.next) != NULL)
			ipq.next->prev = fp;
		ipq.next = fp;
		fp->prev = &ipq;


		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ipqe->ipqe_ip->ip_p;
		fp->ipq_id = ipqe->ipqe_ip->ip_id;
		//fp->ipq_next = fp->ipq_prev = (struct ipasfrag *)fp;
		fp->ipq_fragq.first = NULL;
		fp->ipq_src = ipqe->ipqe_ip->ip_src;
		fp->ipq_dst = ipqe->ipqe_ip->ip_dst;
		//q = (struct ipasfrag *)fp;
		p = NULL;
		goto insert;
	}

//	for (q=fp->ipq_next; q != (struct ipasfrag *)fp; q=q->ipf_next)
//		if (q->ip_off > ip->ip_off)
//			break;

	for (p=NULL, q=fp->ipq_fragq.first; q != NULL;
			p=q, q=q->next){
		if (q->ipqe_ip->ip_off > ipqe->ipqe_ip->ip_off)
			break;
	}

	if (p != NULL){
		i = ntohs(p->ipqe_ip->ip_off) + ntohs(p->ipqe_ip->ip_len) -
				    ntohs(ipqe->ipqe_ip->ip_off);
		if (i > 0){
			if (i >= ntohs(ipqe->ipqe_ip->ip_len))
				goto dropfrag;

			m_adj(ipqe->ipqe_m, i);
			ipqe->ipqe_ip->ip_off =
			    htons(ntohs(ipqe->ipqe_ip->ip_off) + i);
			ipqe->ipqe_ip->ip_len =
			    htons(ntohs(ipqe->ipqe_ip->ip_len) - i);
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL &&
	    ntohs(ipqe->ipqe_ip->ip_off) + ntohs(ipqe->ipqe_ip->ip_len) >
	    ntohs(q->ipqe_ip->ip_off); q = nq) {
		i = (ntohs(ipqe->ipqe_ip->ip_off) +
		    ntohs(ipqe->ipqe_ip->ip_len)) - ntohs(q->ipqe_ip->ip_off);
		if (i < ntohs(q->ipqe_ip->ip_len)) {
			q->ipqe_ip->ip_len =
			    htons(ntohs(q->ipqe_ip->ip_len) - i);
			q->ipqe_ip->ip_off =
			    htons(ntohs(q->ipqe_ip->ip_off) + i);
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = q->next;
		m_freem(q->ipqe_m);
		if (q->next != NULL)
			q->next->prev = q->prev;
		q->prev->next = q->next;
		kfree((void *)q);
	}

insert:
	if (p == NULL){
		//insert head
		if ((ipqe->next = fp->ipq_fragq.first) != NULL)
			fp->ipq_fragq.first->prev = ipqe;
		fp->ipq_fragq.first = ipqe;
		ipqe->prev = fp->ipq_fragq.first;
	} else {
		if ((ipqe->next = p->next) != NULL)
			p->next->prev = ipqe;
		p->next = ipqe;
		ipqe->prev = p;
	}

	next = 0;
	for (p=NULL, q=fp->ipq_fragq.first; q != NULL;
			p=q, q=q->next){
		if (ntohs(q->ipqe_ip->ip_off) != next)
			return NULL;
		next += ntohs(q->ipqe_ip->ip_len);
	}

	if (p->ipq_mff)
		return NULL;

	q = fp->ipq_fragq.first;
	ip = q->ipqe_ip;

	if ((next + (ip->ip_hl << 2)) > IP_MAXPACKET) {
		ipstat.ips_toolong++;
		ip_freef(fp);
		return NULL;
	}

	m = q->ipqe_m;
	t = m->m_next;
	m->m_next = NULL;
	m_cat(m, t);
	nq = q->next;
	kfree((void *)q);

	for (q = nq; q != NULL; q = nq) {
		t = q->ipqe_m;
		nq = q->next;
		kfree((void *)q);
		m_cat(m, t);
	}

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip->ip_len = htons(next);
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;

	if (fp->next != NULL){
		fp->next->prev = fp->prev;
	}
	fp->prev->next = fp->next;

	kfree((void *)fp);
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);

	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}
	return (m);

dropfrag:
	ipstat.ips_fragdropped++;
	m_freem(m);
	kfree((void *)ipqe);
	return NULL;
}

/*
 * IPv4 local-delivery routine.
 *
 * If fragmented try to reassemble.  Pass to next level.
 */
void
ip_ours(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct ipq *fp;
	struct ipqent *ipqe;
	int mff, hlen;

	LOG_INFO("in");

	hlen = ip->ip_hl << 2;

	if (ip->ip_off & ~IP_DF){
		if (m->m_flags & M_EXT){
			if ((m = m_pullup(m, sizeof(struct ip))) == NULL){
				ipstat.ips_toosmall++;
				return;
			}

			ip = mtod(m, struct ip *);
		}

		for (fp = ipq.next; fp != &ipq; fp = fp->next){
			if (ip->ip_id == fp->ipq_id &&
					ip->ip_src.s_addr == fp->ipq_src.s_addr &&
					ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
					ip->ip_p == fp->ipq_p)
				goto found;
		}

		fp = NULL;
found:
		ip->ip_len -= hlen;
		mff = (ip->ip_off & IP_MF) != 0;
		if (mff){
			if (ip->ip_len == 0 ||
					(ip->ip_len & 0x07) != 0){
				ipstat.ips_badfrags++;
				goto bad;
			}
		}
		ip->ip_off <<= 3;
//		((struct ipasfrag *)ip)->ipf_mff &= ~1;
//		if (ip->ip_off & IP_MF)
//			((struct ipasfrag *)ip)->ipf_mff |= 1;

		if (mff || ip->ip_off){
			ipstat.ips_fragments++;

			ipqe = kmalloc(sizeof (*ipqe));
			if (ipqe == NULL){
				ipstat.ips_rcvmemdrop++;
				goto bad;
			}

			ipqe->ipq_mff = mff;
			ipqe->ipqe_m = m;
			ipqe->ipqe_ip = ip;
			m = ip_reass(ipqe, fp);
			if (m == NULL)
				return;
			ipstat.ips_reassembled++;
			ip = mtod(m, struct ip *);
			hlen = ip->ip_hl << 2;
			ip->ip_len = (ip->ip_len + hlen);
		}else if (fp)
			ip_freef(fp);
	} else
		ip->ip_len -= hlen;

	ipstat.ips_delivered++;
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen, NULL, 0);
	LOG_INFO("end");
	return;
bad:
	LOG_INFO("ip discard packet.");
	m_freem(m);
}
int
in_ouraddr(struct in_addr ina, struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct in_ifaddr *ia;
	for (ia = in_ifaddr; ia; ia = ia->ia_next) {
#define	satosin(sa)	((struct sockaddr_in *)(sa))

		if (IA_SIN(ia)->sin_addr.s_addr == ip->ip_dst.s_addr)
			return 1;
		if ((ia->ia_ifp->if_flags & IFF_UP) == 0)
			return 0;
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST)) {
			u32 t;

			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    ip->ip_dst.s_addr)
				return 1;
			if (ip->ip_dst.s_addr == ia->ia_netbroadcast.s_addr)
				return 1;
			/*
			 * Look for all-0's host part (old broadcast addr),
			 * either for subnet or net.
			 */
			t = ntohl(ip->ip_dst.s_addr);
			if (t == ia->ia_subnet)
				return 1;
			if (t == ia->ia_net)
				return 1;
		}
	}
	return 0;
}


void ipv4_input(struct mbuf *m)
{
	struct ip *ip;
	int hlen, len;
	if (in_ifaddr == NULL)
		goto bad;

	LOG_INFO("in");
	ipstat.ips_total++;

	if (m->m_len < sizeof(struct ip) &&
			(m=m_pullup(m, sizeof(struct ip))) == NULL){
		ipstat.ips_toosmall++;
		LOG_WARN("packet too small");
		return;
	}

	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION){
		ipstat.ips_badvers++;
		LOG_WARN("bad ip version");
		goto bad;
	}

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)){
		ipstat.ips_badhlen++;
		LOG_WARN("bad ip head len");
		goto bad;
	}

	if (hlen > m->m_len){
		if ((m=m_pullup(m, hlen)) == NULL){
			ipstat.ips_badhlen++;
			goto bad;
		}
		ip = mtod(m, struct ip *);
	}
	//checksum should be 0 for a valid packet
	if ((ip->ip_sum = in_cksum(m, hlen)) != 0){
		ipstat.ips_badsum++;
		LOG_WARN("ip check sum invalid");
		goto bad;
	}

	len = ntohs(ip->ip_len);
	ip->ip_len = len;
	ip->ip_id = ntohs(ip->ip_id);
	ip->ip_off = ntohs(ip->ip_off);
	LOG_DEBG("ip len:%d", len);
	if (len < hlen){
		ipstat.ips_badlen++;
		goto bad;
	}
	//we lost some bytes? drop this packet
	if (m->m_pkthdr.len < len){
		ipstat.ips_tooshort++;
		goto bad;
	}
	//additional bytes may be filled by linker layer,
	//to reach the least count of an ip packet,
	//here, we trim it
	if (m->m_pkthdr.len > len){
		if (m->m_len == m->m_pkthdr.len){
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else {
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
		}
	}

	ip_nhops = 0;

	//check ip options if it has
	if (hlen > sizeof(struct ip) && ip_do_options(m))
		return;

	//
	if (in_ouraddr(ip->ip_dst, m)){
		ip_ours(m);
		return;
	}

	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))){
		struct in_multi *inm;
		/*
		 * see if we are in the multicast group
		 */
		IN_LOOKUP_MULTI(ip->ip_dst, m->m_pkthdr.rcvif, inm);
		if (inm == NULL){
			ipstat.ips_notmember++;
			if (!IN_LOCALGROUP(ip->ip_dst.s_addr)){
				ipstat.ips_cantforward++;
			}
			goto bad;
		}

		ip_ours(m);
		return;
	}

	if (ip->ip_dst.s_addr == INADDR_BROADCAST ||
			ip->ip_dst.s_addr == INADDR_ANY){
		ip_ours(m);
		return;
	}

	if (ipforwarding == 0){
		ipstat.ips_cantforward++;
		goto bad;
	}

	ip_forward(m, 0);
	LOG_INFO("end");
	return;
bad:
	m_freem(m);
}


/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
ip_slowtimo()
{
	struct ipq *fp;

	fp = ipq.next;
	if (fp == 0) {
		return;
	}
	while (fp != &ipq) {
		--fp->ipq_ttl;
		fp = fp->next;
		if (fp->prev->ipq_ttl == 0) {
			ipstat.ips_fragtimeout++;
			ip_freef(fp->prev);
		}
	}
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain()
{

	while (ipq.next != &ipq) {
		ipstat.ips_fragdropped++;
		ip_freef(ipq.next);
	}
}

#define GATEWAY
/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init()
{
	struct protosw *pr;
	int i;

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;
	ipq.next = ipq.prev = &ipq;
	ip_id = xtime.ts_sec & 0xffff;
	ipintrq.ifq_maxlen = ipqmaxlen;
#ifdef GATEWAY
	i = (if_index + 1) * (if_index + 1) * sizeof (u32);
	ip_ifmatrix = (u32 *) kzalloc(i);
#endif
}




void ipintr()
{
	struct mbuf *m;
	int x;
	for(;;){
		local_irq_save(&x);
		IF_DEQUEUE(&ipintrq, m);
		local_irq_restore(x);
		if (m == NULL)
			return;

		ipv4_input(m);
	}

}
