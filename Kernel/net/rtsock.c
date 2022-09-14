/*
 * rtsock.c
 *
 *  Created on: Aug 14, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <kernel/net/if.h>
#include <kernel/net/route.h>
#include <kernel/sys/socket.h>
#include <string.h>
#include <kernel/sys/protosw.h>
#include <kernel/net/raw_cb.h>
#include <kernel/sys/socketval.h>
#include <kernel/malloc.h>
#include <kernel/kthread.h>

struct	sockaddr route_dst = { 2, PF_ROUTE, };
struct	sockaddr route_src = { 2, PF_ROUTE, };
struct	sockproto route_proto = { PF_ROUTE, };

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

struct walkarg {
	int	w_op, w_arg, w_given, w_needed, w_tmemsize;
	caddr_t	w_where, w_tmem;
};

#define dst	info.rti_info[RTAX_DST]
#define gate	info.rti_info[RTAX_GATEWAY]
#define netmask	info.rti_info[RTAX_NETMASK]
#define genmask	info.rti_info[RTAX_GENMASK]
#define ifpaddr	info.rti_info[RTAX_IFP]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

static void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr *sa;
	int i;

	Bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}
/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
void
m_copyback(struct mbuf *m0, int off, int len, caddr_t cp)
{
	int mlen;
	struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == 0)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == NULL)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		Bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == 0) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == NULL)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
}




static struct mbuf *
rt_msg1(int type, struct rt_addrinfo *rtinfo)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	int i;
	struct sockaddr *sa;
	int len, dlen;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (m);
	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if (len > MHLEN)
		LOG_ERROR("len > MKLEN");
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	rtm = mtod(m, struct rt_msghdr *);
	Bzero((caddr_t)rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		m_copyback(m, len, dlen, (caddr_t)sa);
		len += dlen;
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return (NULL);
	}
	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}


/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	struct rt_addrinfo info;
	struct sockaddr *sa;
	int pass;
	struct mbuf *m;
	struct ifnet *ifp = ifa->ifa_ifp;

	LOG_INFO("in");
	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		Bzero((caddr_t)&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr *ifam;
			int ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

			ifaaddr = sa = ifa->ifa_addr;
			ifpaddr = ifp->if_addrlist->ifa_addr;
			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;

			if (rt == NULL)
				continue;
			netmask = rt_mask(rt);
			dst = sa = rt_key(rt);
			gate = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		route_proto.sp_protocol = sa ? sa->sa_family : 0;
		raw_input(m, &route_proto, &route_src, &route_dst);
	}
}


/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occured, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == NULL)
		return;
	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	route_proto.sp_protocol = sa ? sa->sa_family : 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * called by if_up and if_down,
 * to generate a route change message
 * send to right process
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;

	if (route_cb.any_count == 0)
		return;
	Bzero((caddr_t)&info, sizeof(info));
	m = rt_msg1(RTM_IFINFO, &info);
	if (m == NULL)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}


void
rt_setmetrics(u32 which, struct rt_metrics *in, struct rt_metrics *out)
{
#define metric(f, e) if (which & (f)) out->e = in->e;
	metric(RTV_RPIPE, rmx_recvpipe);
	metric(RTV_SPIPE, rmx_sendpipe);
	metric(RTV_SSTHRESH, rmx_ssthresh);
	metric(RTV_RTT, rmx_rtt);
	metric(RTV_RTTVAR, rmx_rttvar);
	metric(RTV_HOPCOUNT, rmx_hopcount);
	metric(RTV_MTU, rmx_mtu);
	metric(RTV_EXPIRE, rmx_expire);
#undef metric
}


static int
rt_msg2(int type, struct rt_addrinfo *rtinfo,
		caddr_t cp, struct walkarg *w)
{
	int i;
	int len, dlen, second_time = 0;
	caddr_t cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if ((cp0 = cp) != NULL)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == 0)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		if (cp) {
			Bcopy((caddr_t)sa, cp, (unsigned)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	if (cp == 0 && w != NULL && !second_time) {
		struct walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem)
					Free(rw->w_tmem);
				if ((rw->w_tmem = (caddr_t)kmalloc(len)) != NULL)
					rw->w_tmemsize = len;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else
				rw->w_where = 0;
		}
	}
	if (cp) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
	return (len);
}

int
route_usrreq(struct socket *so, int req,
		struct mbuf *m, struct mbuf *nam,
		struct mbuf *control)
{
	int error = 0;
	struct rawcb *rp = sotorawcb(so);

	if (req == PRU_ATTACH) {
		R_Malloc(rp, struct rawcb *, sizeof(*rp));
		if ((so->so_pcb = (caddr_t)rp) != NULL)
			Bzero(so->so_pcb, sizeof(*rp));

	}
	if (req == PRU_DETACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (af == AF_INET)
			route_cb.ip_count--;
		else if (af == AF_NS)
			route_cb.ns_count--;
		else if (af == AF_ISO)
			route_cb.iso_count--;
		route_cb.any_count--;
	}

	error = raw_usrreq(so, req, m, nam, control);
	rp = sotorawcb(so);
	if (req == PRU_ATTACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (error) {
			Free((caddr_t)rp);
			return (error);
		}
		if (af == AF_INET)
			route_cb.ip_count++;
		else if (af == AF_NS)
			route_cb.ns_count++;
		else if (af == AF_ISO)
			route_cb.iso_count++;
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	return (error);
}


int
route_output(struct mbuf *m, struct socket *so)
{
	struct rt_msghdr *rtm = NULL;
	struct rtentry *rt = NULL;
	struct rtentry *saved_nrt = NULL;
	struct radix_node_head *rnh;
	struct rt_addrinfo info;
	int len, error = 0;
	struct ifnet *ifp = NULL;
	struct ifaddr *ifa = NULL;

#define senderr(e) { error = e; goto flush;}
	if (m == 0 || ((m->m_len < sizeof(long)) &&
		       (m = m_pullup(m, sizeof(long))) == NULL))
		return (-ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		LOG_INFO("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		dst = 0;
		senderr(-EINVAL);
	}
	R_Malloc(rtm, struct rt_msghdr *, len);
	if (rtm == NULL) {
		dst = NULL;
		senderr(-ENOBUFS);
	}
	m_copydata(m, 0, len, (caddr_t)rtm);
	if (rtm->rtm_version != RTM_VERSION) {
		dst = NULL;
		senderr(-EPROTONOSUPPORT);
	}
	rtm->rtm_pid = current->pid;
	info.rti_addrs = rtm->rtm_addrs;
	rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, &info);
	if (dst == NULL)
		senderr(-EINVAL);
	if (genmask) {
		struct radix_node *t;
		t = rn_addmask((caddr_t)genmask, 0, 1);
		if (t && Bcmp(genmask, t->rn_key, *(u8 *)genmask) == 0)
			genmask = (struct sockaddr *)(t->rn_key);
		else
			senderr(-ENOBUFS);
	}
	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (gate == NULL)
			senderr(-EINVAL);
		error = rtrequest(RTM_ADD, dst, gate, netmask,
					rtm->rtm_flags, &saved_nrt);
		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits,
				&rtm->rtm_rmx, &saved_nrt->rt_rmx);
			saved_nrt->rt_refcnt--;
			saved_nrt->rt_genmask = genmask;
		}
		break;

	case RTM_DELETE:
		error = rtrequest(RTM_DELETE, dst, gate, netmask,
				rtm->rtm_flags, &saved_nrt);
		if (error == 0) {
			(rt = saved_nrt)->rt_refcnt++;
			goto report;
		}
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		if ((rnh = rt_tables[dst->sa_family]) == 0) {
			senderr(-EAFNOSUPPORT);
		} else if ((rt = (struct rtentry *)
				rnh->rnh_lookup(dst, netmask, rnh)) != NULL)
			rt->rt_refcnt++;
		else
			senderr(-ESRCH);
		switch(rtm->rtm_type) {

		case RTM_GET:
		report:
			dst = rt_key(rt);
			gate = rt->rt_gateway;
			netmask = rt_mask(rt);
			genmask = rt->rt_genmask;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				if ((ifp = rt->rt_ifp) != NULL) {
					ifpaddr = ifp->if_addrlist->ifa_addr;
					ifaaddr = rt->rt_ifa->ifa_addr;
					if (ifp->if_flags & IFF_POINTOPOINT)
						brdaddr = rt->rt_ifa->ifa_dstaddr;
					else
						brdaddr = NULL;
					rtm->rtm_index = ifp->if_index;
				} else {
					ifpaddr = NULL;
					ifaaddr = NULL;
			    }
			}
			len = rt_msg2(rtm->rtm_type, &info, (caddr_t)0,
				(struct walkarg *)0);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr *new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == NULL)
					senderr(-ENOBUFS);
				Bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			(void)rt_msg2(rtm->rtm_type, &info, (caddr_t)rtm,
				(struct walkarg *)0);
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_rmx = rt->rt_rmx;
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			if (gate && rt_setgate(rt, rt_key(rt), gate))
				senderr(-EDQUOT);
			/* new gateway could require new ifaddr, ifp;
			   flags may also be different; ifp may be specified
			   by ll sockaddr when protocol address is ambiguous */
			if (ifpaddr && (ifa = ifa_ifwithnet(ifpaddr)) &&
			    (ifp = ifa->ifa_ifp))
				ifa = ifaof_ifpforaddr(ifaaddr ? ifaaddr : gate,
							ifp);
			else if ((ifaaddr && (ifa = ifa_ifwithaddr(ifaaddr))) ||
				 (ifa = ifa_ifwithroute(rt->rt_flags,
							rt_key(rt), gate)))
				ifp = ifa->ifa_ifp;
			if (ifa) {
				struct ifaddr *oifa = rt->rt_ifa;
				if (oifa != ifa) {
				    if (oifa && oifa->ifa_rtrequest)
					oifa->ifa_rtrequest(RTM_DELETE,
								rt, gate);
				    IFAFREE(rt->rt_ifa);
				    rt->rt_ifa = ifa;
				    ifa->ifa_refcnt++;
				    rt->rt_ifp = ifp;
				}
			}
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
					&rt->rt_rmx);
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
			       rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, gate);
			if (genmask)
				rt->rt_genmask = genmask;
			/*
			 * Fall into
			 */
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
				(rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			break;
		}
		break;

	default:
		senderr(-EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else
			rtm->rtm_flags |= RTF_DONE;
	}
	if (rt)
		rtfree(rt);
    {
		struct rawcb *rp = 0;
		/*
		 * Check to see if we don't want our own messages.
		 */
		if ((so->so_options & SO_USELOOPBACK) == 0) {
			if (route_cb.any_count <= 1) {
				if (rtm)
					Free(rtm);
				m_freem(m);
				return (error);
			}
			/* There is another listener, so construct message */
			rp = sotorawcb(so);
		}
		if (rtm) {
			m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm);
			Free(rtm);
		}
		if (rp)
			rp->rcb_proto.sp_family = 0; /* Avoid us */
		if (dst)
			route_proto.sp_protocol = dst->sa_family;
		raw_input(m, &route_proto, &route_src, &route_dst);
		if (rp)
			rp->rcb_proto.sp_family = PF_ROUTE;
    }
	return (error);
}

int
sysctl_rtable(int *name, int namelen, caddr_t where,
		size_t given, caddr_t *new, size_t newlen)
{

	int error = -EINVAL;
//	struct radix_node_head *rnh;
//	int	i, s,
//	u8  af;
//	struct	walkarg w;
//
//	if (new)
//		return (-EPERM);
//	if (namelen != 3)
//		return (-EINVAL);
//	af = name[0];
//	Bzero(&w, sizeof(w));
//	w.w_where = where;
//	w.w_given = *given;
//	w.w_needed = 0 - w.w_given;
//	w.w_op = name[1];
//	w.w_arg = name[2];
//
//	switch (w.w_op) {
//	case NET_RT_DUMP:
//	case NET_RT_FLAGS:
//		for (i = 1; i <= AF_MAX; i++)
//			if ((rnh = rt_tables[i]) && (af == 0 || af == i) &&
//			    (error = rnh->rnh_walktree(rnh,
//							sysctl_dumpentry, &w)))
//				break;
//		break;
//
//	case NET_RT_IFLIST:
//		error = sysctl_iflist(af, &w);
//	}
//
//	if (w.w_tmem)
//		free(w.w_tmem, M_RTABLE);
//	w.w_needed += w.w_given;
//	if (where) {
//		*given = w.w_where - where;
//		if (*given < w.w_needed)
//			return (-ENOMEM);
//	} else {
//		*given = (11 * w.w_needed) / 10;
//	}
	return (error);
}


/*
 * Definitions of protocols supported in the ROUTE domain.
 */

extern	struct domain routedomain;		/* or at least forward */

struct protosw routesw[] = {
	{
		SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR,
		raw_input,	route_output,	raw_ctlinput,	0,
		route_usrreq,
		raw_init,	0,		0,		0,
		sysctl_rtable,
	}
};

struct domain routedomain = {
	PF_ROUTE, "route", route_init, 0, 0,
    routesw, &routesw[sizeof(routesw)/sizeof(routesw[0])]
};













