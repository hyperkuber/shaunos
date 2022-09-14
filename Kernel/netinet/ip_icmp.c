/*
 * ip_icmp.c
 *
 *  Created on: Jul 24, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */


#include <kernel/mbuf.h>
#include <kernel/sys/netinet/ip.h>
#include <kernel/sys/netinet/ip_icmp.h>
#include <kernel/sys/protosw.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/net/route.h>
#include <kernel/types.h>
#include <kernel/net/if.h>
#include <kernel/time.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/kernel.h>
#include <string.h>
#include <kernel/sys/netinet/in.h>

static struct sockaddr_in icmpsrc = {sizeof(struct sockaddr_in), AF_INET};
static struct sockaddr_in icmpdst = {sizeof(struct sockaddr_in), AF_INET};
static struct sockaddr_in icmpgw = {sizeof(struct sockaddr_in), AF_INET};

struct sockaddr_in	icmpask	= {8, 0};

extern struct 	in_ifaddr *in_ifaddr;
extern struct protosw inetsw[];
extern u8	ip_protox[IPPROTO_MAX];

int	icmpmaskrepl = 0;
void icmp_reflect(struct mbuf *m);

extern int ip_output(struct mbuf *m0, struct mbuf *opt,
		struct route *ro, int flags,
		struct ip_moptions *imo);
extern int in_cksum(struct mbuf *m, int len);


void icmp_input(struct mbuf *m, int hlen)
{
	struct icmp *icp;
	struct ip *ip = mtod(m, struct ip *);
	int icmplen;
	int i;
	struct in_ifaddr *ia;
	void (*ctlfunc)(int, struct sockaddr *, struct ip *);
	int code;
	//extern u8	ip_protox[];

	LOG_INFO("in");

	icmplen = ip->ip_len;

	if (icmplen < ICMP_MINLEN){
		icmpstat.icps_tooshort++;
		LOG_INFO("icmplen too short:%d", icmpstat);
		goto freeit;
	}

	i = hlen + min(icmplen, ICMP_ADVLENMIN);

	if (m->m_len < i && (m = m_pullup(m, i)) == NULL){
		LOG_INFO("icmp too short ,i:%d", i);
		icmpstat.icps_tooshort++;
		return;
	}
	ip = mtod(m, struct ip *);
	m->m_len -= hlen;
	m->m_data += hlen;
	if (in_cksum(m, icmplen)){
		icmpstat.icps_checksum++;
		LOG_INFO("icmp checksum error");
		goto freeit;
	}

	m->m_len += hlen;
	m->m_data -= hlen;

	icp = (struct icmp *)(mtod(m, caddr_t) + hlen);

	if (icp->icmp_type > ICMP_MAXTYPE)
		goto raw;

	icmpstat.icps_inhist[icp->icmp_type]++;
	code = icp->icmp_code;

	switch (icp->icmp_type){
	case ICMP_UNREACH:
		switch (code){
		case ICMP_UNREACH_NET:
		case ICMP_UNREACH_HOST:
		case ICMP_UNREACH_PROTOCOL:
		case ICMP_UNREACH_PORT:
		case ICMP_UNREACH_SRCFAIL:
			code += PRC_UNREACH_NET;
			break;
		case ICMP_UNREACH_NEEDFRAG:
			code = PRC_MSGSIZE;
			break;
		case ICMP_UNREACH_NET_UNKNOWN:
		case ICMP_UNREACH_NET_PROHIB:
		case ICMP_UNREACH_TOSNET:
			code = PRC_UNREACH_NET;
			break;
		case ICMP_UNREACH_HOST_UNKNOWN:
		case ICMP_UNREACH_ISOLATED:
		case ICMP_UNREACH_HOST_PROHIB:
		case ICMP_UNREACH_TOSHOST:
			code = PRC_UNREACH_HOST;
			break;
		default:
			goto badcode;
		}
		goto deliver;
	case ICMP_TIMXCEED:
		if (code > 1)
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;
	case ICMP_PARAMPROB:
		if (code > 1)
			goto badcode;
		code = PRC_PARAMPROB;
		goto deliver;
	case ICMP_SOURCEQUENCH:
		if(code)
			goto badcode;
		code = PRC_QUENCH;
deliver:
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp)	||
				icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)){
			icmpstat.icps_badlen++;
			goto freeit;
		}

		if (IN_MULTICAST(icp->icmp_ip.ip_dst.s_addr))
			goto badcode;

		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		ctlfunc = inetsw[ip_protox[icp->icmp_ip.ip_p]].pr_ctlinput;
		if (ctlfunc)
			(*ctlfunc)(code, (struct sockaddr *)&icmpsrc, &icp->icmp_ip);

		break;
badcode:
		icmpstat.icps_badcode++;
		break;

	case ICMP_ECHO:
		icp->icmp_type = ICMP_ECHOREPLY;
		goto reflect;

	case ICMP_TSTAMP:
		if (icmplen < ICMP_TSLEN) {
			icmpstat.icps_badlen++;
			break;
		}
		icp->icmp_type = ICMP_TSTAMPREPLY;
		icp->icmp_rtime = iptime();
		icp->icmp_ttime = icp->icmp_rtime;
		goto reflect;

	case ICMP_MASKREQ:
#define satosin(sa)	((struct sockaddr_in *)(sa))
		if (icmpmaskrepl == 0)
			break;
		if (icmplen < ICMP_MASKLEN)
			break;
		switch (ip->ip_dst.s_addr){
		case INADDR_BROADCAST:
		case INADDR_ANY:
			icmpdst.sin_addr = ip->ip_src;
			break;
		default:
			icmpdst.sin_addr = ip->ip_dst;
		}
		ia = (struct in_ifaddr *)ifaof_ifpforaddr(
				(struct sockaddr *)&icmpdst, m->m_pkthdr.rcvif);
		if (ia == NULL)
			break;
		icp->icmp_type = ICMP_MASKREPLY;
		icp->icmp_mask = ia->ia_sockmask.sin_addr.s_addr;
		if (ip->ip_src.s_addr == 0){
			if (ia->ia_ifp->if_flags & IFF_BROADCAST)
				ip->ip_src = satosin(&ia->ia_broadaddr)->sin_addr;
			else if (ia->ia_ifp->if_flags & IFF_POINTOPOINT)
				ip->ip_src = satosin(&ia->ia_dstaddr)->sin_addr;
		}
reflect:
		ip->ip_len += hlen;
		icmpstat.icps_reflect++;
		icmpstat.icps_outhist[icp->icmp_type]++;
		icmp_reflect(m);
		return;

	case ICMP_REDIRECT:
		if (code > 3)
			goto badcode;
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
				icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)){
			icmpstat.icps_badlen++;
			break;
		}

		icmpgw.sin_addr = ip->ip_src;
		icmpdst.sin_addr = icp->icmp_gwaddr;
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		rtredirect((struct sockaddr *)&icmpsrc,
				(struct sockaddr *)&icmpdst,
				(struct sockaddr *)NULL, RTF_GATEWAY | RTF_HOST,
				(struct sockaddr *)&icmpgw, NULL);
		pfctlinput(PRC_REDIRECT_HOST, (struct sockaddr *)&icmpsrc);
		break;

	case ICMP_ECHOREPLY:
	case ICMP_ROUTERADVERT:
	case ICMP_ROUTERSOLICIT:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQREPLY:
	case ICMP_MASKREPLY:
	default:
		break;
	}

raw:
	LOG_INFO("end");
	rip_input(m);
	return;
freeit:
	LOG_INFO("icmp free m:%x",(u32)m);
	m_freem(m);
}


/*
 * Send an icmp packet back to the ip level,
 * after supplying a checksum.
 */
void
icmp_send(struct mbuf *m, struct mbuf *opts)
{
	struct ip *ip = mtod(m, struct ip *);
	int hlen;
	struct icmp *icp;
	LOG_INFO("in");
	hlen = ip->ip_hl << 2;
	m->m_data += hlen;
	m->m_len -= hlen;
	icp = mtod(m, struct icmp *);
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ip->ip_len - hlen);
	m->m_data -= hlen;
	m->m_len += hlen;

	(void) ip_output(m, opts, NULL, 0, NULL);
	LOG_INFO("end");
}

void icmp_reflect(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct in_ifaddr *ia;
	struct in_addr t;
	struct mbuf *opts = NULL, *ip_srcroute();

	int optlen = (ip->ip_hl << 2) - sizeof(struct ip);

	LOG_INFO("in");

	if (!in_canforward(ip->ip_src) &&
			((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) !=
					(IN_LOOPBACKNET << IN_CLASSA_NSHIFT))){
		m_free(m);
		goto done;
	}
	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;


	for (ia = in_ifaddr; ia; ia = ia->ia_next) {
		if (t.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST) &&
		    t.s_addr == satosin(&ia->ia_broadaddr)->sin_addr.s_addr)
			break;
	}
	icmpdst.sin_addr = t;
	if (ia == (struct in_ifaddr *)0)
		ia = (struct in_ifaddr *)ifaof_ifpforaddr(
			(struct sockaddr *)&icmpdst, m->m_pkthdr.rcvif);
	/*
	 * The following happens if the packet was not addressed to us,
	 * and was received on an interface with no IP address.
	 */
	if (ia == (struct in_ifaddr *)0)
		ia = in_ifaddr;
	t = IA_SIN(ia)->sin_addr;
	ip->ip_src = t;
	ip->ip_ttl = MAXTTL;

	if (optlen > 0) {
		u8 *cp;
		int opt, cnt;
		u32 len;

		/*
		 * Retrieve any source routing from the incoming packet;
		 * add on any record-route or timestamp options.
		 */
		cp = (u8 *) (ip + 1);
		if ((opts = ip_srcroute()) == NULL &&
		    (MGETHDR(opts, M_DONTWAIT, MT_HEADER))) {
			opts->m_len = sizeof(struct in_addr);
			mtod(opts, struct in_addr *)->s_addr = 0;
		}
		if (opts) {
		    for (cnt = optlen; cnt > 0; cnt -= len, cp += len) {
			    opt = cp[IPOPT_OPTVAL];
			    if (opt == IPOPT_EOL)
				    break;
			    if (opt == IPOPT_NOP)
				    len = 1;
			    else {
				    len = cp[IPOPT_OLEN];
				    if (len <= 0 || len > cnt)
					    break;
			    }
			    /*
			     * Should check for overflow, but it "can't happen"
			     */
			    if (opt == IPOPT_RR || opt == IPOPT_TS ||
				opt == IPOPT_SECURITY) {
				    memcpy((void *)(mtod(opts, caddr_t) + opts->m_len),
					(caddr_t)cp, len);
				    opts->m_len += len;
			    }
		    }
		    /* Terminate & pad, if necessary */
		    if ((cnt = opts->m_len % 4) != 0) {
			    for (; cnt < 4; cnt++) {
				    *(mtod(opts, caddr_t) + opts->m_len) =
					IPOPT_EOL;
				    opts->m_len++;
			    }
		    }

		}
		/*
		 * Now strip out original options by copying rest of first
		 * mbuf's data back, and adjust the IP length.
		 */
		ip->ip_len -= optlen;
		ip->ip_hl = sizeof(struct ip) >> 2;
		m->m_len -= optlen;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= optlen;
		optlen += sizeof(struct ip);
		memcpy((void *)(ip + 1), (caddr_t)ip + optlen,
			 (unsigned)(m->m_len - sizeof(struct ip)));
	}
	m->m_flags &= ~(M_BCAST|M_MCAST);
	icmp_send(m, opts);
done:
	LOG_INFO("icmp done");
	if (opts)
		(void)m_free(opts);
}

void
icmp_error(struct mbuf *n, int type, int code, n_long dest, struct ifnet *destifp)
{
	struct ip *oip = mtod(n, struct ip *), *nip;
	unsigned oiplen = oip->ip_hl << 2;
	struct icmp *icp;
	struct mbuf *m;
	unsigned icmplen;


	LOG_INFO("in");
	if (type != ICMP_REDIRECT)
		icmpstat.icps_error++;


	if (oip->ip_off & ~(IP_MF|IP_DF))
		goto freeit;

	if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
			n->m_len >= oiplen + ICMP_MINLEN &&
			!ICMP_INFOTYPE(((struct icmp *)((caddr_t)oip+oiplen))->icmp_type)){
		icmpstat.icps_oldicmp++;
		goto freeit;
	}

	if (n->m_flags & (M_BCAST | M_MCAST))
		goto freeit;

	icmplen = oiplen + min(8, ntohs(oip->ip_len));

	if (icmplen + ICMP_MINLEN > MCLBYTES)
		icmplen = MCLBYTES - ICMP_MINLEN - sizeof(struct ip);

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m && (sizeof(struct ip) + icmplen + ICMP_MINLEN) > MHLEN){
		MCLGET(m, M_DONTWAIT);
		if((m->m_flags & M_EXT) == 0){
			m_freem(m);
			m = NULL;
		}
	}

	if (m == NULL)
		goto freeit;

	m->m_len = icmplen + ICMP_MINLEN;
	if ((m->m_flags & M_EXT) == 0)
		MH_ALIGN(m, m->m_len);

	icp = mtod(m, struct icmp *);
	if ((u32)type > ICMP_MAXTYPE)
		LOG_ERROR("icmp error");
	icmpstat.icps_outhist[type]++;
	icp->icmp_type = type;

	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr.s_addr = dest;
	else {
		icp->icmp_void = 0;
		if (type == ICMP_PARAMPROB){
			icp->icmp_pptr = code;
			code = 0;
		} else if (type == ICMP_UNREACH &&
				code == ICMP_UNREACH_NEEDFRAG &&
				destifp)
			icp->icmp_nextmtu = htons(destifp->if_mtu);
	}
	icp->icmp_code = code;

	//m_copydata(n, 0, icmplen, (caddr_t)&icp->icmp_ip);
	//TODO:if icmplen > 128, i.e if (m->m_flags & M_EXT)
	memcpy((void *)&icp->icmp_ip, (void *)oip, icmplen);

	if((m->m_flags & M_EXT) == 0 &&
			m->m_data - sizeof(struct ip) < m->m_pktdat)
		LOG_ERROR("icmp len");

	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = n->m_pkthdr.rcvif;

	nip = mtod(m, struct ip *);
	nip->ip_hl = sizeof(struct ip) >> 2;
	nip->ip_tos = 0;						//RFC792 & RFC 1122
	nip->ip_len = htons(m->m_len);
	nip->ip_off = 0;
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_src = oip->ip_src;
	nip->ip_dst = oip->ip_dst;
	//ip_v,ip_id set in ip_output;
	//ip_ttl set in icmp_reflect
	icmp_reflect(m);
freeit:
	m_freem(n);
}




n_time
iptime(void)
{
	struct timeval atv;
	u32 t;

	ti_get_timeval(&atv);
	t = (atv.tv_sec % (24*60*60)) * 1000 + atv.tv_usec / 1000;
	return (htonl(t));
}




