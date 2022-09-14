/*
 * route.c
 *
 *  Created on: Aug 1, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/net/route.h>
#include <kernel/net/radix.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/sys/domain.h>

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */
struct	rtstat rtstat;
#define	SA(p) ((struct sockaddr *)(p))


int rtrequest(int req,
		struct sockaddr *dst,
		struct sockaddr *gateway,
		struct sockaddr *netmask,
		int flags,
		struct rtentry **ret_nrt);

int
rt_setgate(struct rtentry *rt0, struct sockaddr *dst,
		struct sockaddr *gate);

void
rtable_init(void **table)
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
}

void
route_init()
{
	LOG_DEBG("in");
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
	LOG_DEBG("end");
}


struct rtentry *
rtalloc1(struct sockaddr *dst, int report)
{
	struct radix_node_head *rnh = rt_tables[dst->sa_family];
	struct rtentry *rt;
	struct radix_node *rn;
	struct rtentry *newrt = 0;
	struct rt_addrinfo info;
	int  err = 0, msgtype = RTM_MISS;

	LOG_DEBG("in");
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		LOG_DEBG("rn:%x", rn);
		if (report && (rt->rt_flags & RTF_CLONING)) {
			err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt);
			if (err) {
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
				msgtype = RTM_RESOLVE;
				goto miss;
			}
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
	miss:	if (report) {
			Bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	LOG_DEBG("end, newrt:%x", (u32)newrt);
	return (newrt);
}



void
rtalloc(struct route *ro)
{
	LOG_DEBG("in");
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
	LOG_DEBG("ro_rt:%x", ro->ro_rt);
}

void
ifafree(struct ifaddr *ifa)
{
	if (ifa == NULL)
		LOG_ERROR("ifafree");
	if (ifa->ifa_refcnt == 0)
		kfree(ifa);
	else
		ifa->ifa_refcnt--;
}

void
rtfree(struct rtentry *rt)
{
	struct ifaddr *ifa;

	if (rt == 0)
		LOG_ERROR("rtfree");
	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			LOG_ERROR ("rtfree 2");
		rttrash--;
		if (rt->rt_refcnt < 0) {
			LOG_INFO("rtfree: %x not freed (neg refs)", rt);
			return;
		}
		ifa = rt->rt_ifa;
		IFAFREE(ifa);
		Free(rt_key(rt));
		Free(rt);
	}
}



void
rtredirect(struct sockaddr *dst,
		struct sockaddr *gateway,
		struct sockaddr *netmask,
		int	flags,
		struct sockaddr *src,
		struct rtentry **rtp)
{
	struct rtentry *rt;
	int error = 0;
	short *stat = NULL;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	if ((ifa = ifa_ifwithnet(gateway)) == NULL){
		error = -ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0);
	if (!(flags & RTF_DONE) && rt &&
			(!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = -EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = -EHOSTUNREACH;
	if (error)
		goto done;

	if (rt == NULL || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;

	if (rt->rt_flags & RTF_GATEWAY){
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)){
			create:
			flags |= RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway, netmask, flags, NULL);
			stat = & rtstat.rts_dynamic;
		} else {
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = -EHOSTUNREACH;
	done:
	if (rt){
		if (rtp && (!error))
			*rtp = rt;
		else
			rtfree(rt);
	}

	out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;

	Bzero((caddr_t)&info, sizeof(info));

	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);

}

struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr	*dst,
		struct sockaddr	*gateway)
{
	struct ifaddr *ifa;
	LOG_DEBG("in");
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST)
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == 0) {
		struct rtentry *rt = rtalloc1(dst, 0);
		if (rt == 0)
			return (0);
		rt->rt_refcnt--;
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	LOG_DEBG("in");
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rt_setgate(struct rtentry *rt0, struct sockaddr *dst,
		struct sockaddr *gate)
{
	caddr_t new, old;
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
	struct rtentry *rt = rt0;

	if (rt->rt_gateway == NULL || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == NULL)
			return 1;
		rt->rt_nodes->rn_key = new;
	} else {
		new = rt->rt_nodes->rn_key;
		old = 0;
	}
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
	}
	if (rt->rt_gwroute) {
		rt = rt->rt_gwroute;
		RTFREE(rt);
		//if (rt->rt_refcnt == 0)
			//rt->rt_gwroute = NULL;
		rt = rt0;
		rt->rt_gwroute = 0;	//???
	}
	if (rt->rt_flags & RTF_GATEWAY) {
		rt->rt_gwroute = rtalloc1(gate, 1);
	}
	return 0;
}



void
rt_maskedcopy(src, dst, netmask)
	struct sockaddr *src, *dst, *netmask;
{
	u8 *cp1 = (u8 *)src;
	u8 *cp2 = (u8 *)dst;
	u8 *cp3 = (u8 *)netmask;
	u8 *cplim = cp2 + *cp3;
	u8 *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;	//already perform & ,
									//for enhance rn_match
	if (cp2 < cplim2)
		Bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

int rtrequest(int req,
		struct sockaddr *dst,
		struct sockaddr *gateway,
		struct sockaddr *netmask,
		int flags,
		struct rtentry **ret_nrt)
{
	int error = 0;
	struct rtentry *rt;
	struct radix_node *rn;
	struct radix_node_head * rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;

	LOG_DEBG("in");
#define error_out(x)	{ error = (x); goto bad;}
	LOG_NOTE("dst family:%d", dst->sa_family);
	if ((rnh = rt_tables[dst->sa_family]) == NULL)
		error_out(-ESRCH);

	if (flags & RTF_HOST)
		netmask = NULL;
	switch(req){
	case RTM_DELETE:
		if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == NULL)
			error_out(-ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			LOG_INFO("delete root route");
		rt = (struct rtentry *)(rn);
		rt->rt_flags &= ~RTF_UP;
		if (rt->rt_gwroute){
			//rt = rt->rt_gwroute;
			RTFREE(rt->rt_gwroute);
			//rt->rt_gwroute = NULL;
		}
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, NULL);
		rttrash++;
		if (ret_nrt)
			*ret_nrt = rt;
		else if (rt->rt_refcnt <= 0){
			rt->rt_refcnt++;
			rtfree(rt);
		}
		break;
	case RTM_RESOLVE:
		if (ret_nrt == NULL || (rt = *ret_nrt) == NULL)
			error_out(-EINVAL);

		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == NULL)
			flags |= RTF_HOST;
		goto makeroute;
	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == NULL)
			error_out(-ENETUNREACH);
makeroute:
		R_Malloc(rt, struct rtentry *, sizeof(*rt));
		if (rt == NULL)
			error_out(-ENOBUFS);
		Bzero(rt, sizeof(*rt));
		rt->rt_flags |= RTF_UP | flags;
		if (rt_setgate(rt, dst, gateway)){
			Free(rt);
			error_out(-ENOBUFS);
		}
		ndst = rt_key(rt);
		if (netmask){
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);

		rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask, rnh, rt->rt_nodes);
		if (rn == NULL){
			if (rt->rt_gwroute)
				rtfree(rt->rt_gwroute);
			Free(rt_key(rt));
			Free(rt);
			error_out(-EEXIST);
		}
		ifa->ifa_refcnt++;
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		if (req == RTM_RESOLVE)
			rt->rt_rmx = (*ret_nrt)->rt_rmx;
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
		if (ret_nrt){
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		break;
	}
bad:
	LOG_DEBG("end, error=%d, rt:%x", -error, (u32)rt);
	return error;
}


int rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	struct rtentry *rt;
	struct sockaddr *dst;
	struct sockaddr *deldst;
	struct mbuf *m = NULL;
	struct rtentry *nrt = NULL;
	int error = 0;

	LOG_INFO("in.");
	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (cmd == RTM_DELETE){
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask){
			m = m_get(M_WAIT, MT_SONAME);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}

		if ((rt = rtalloc1(dst, 0)) != NULL){
			rt->rt_refcnt--;
			if (rt->rt_ifa != ifa){
				if (m)
					m_free(m);
				return (flags & RTF_HOST ? -EHOSTUNREACH : -ENETUNREACH);
			}
		}

	}

	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask, flags | ifa->ifa_flags, &nrt);
	if (m)
		m_free(m);

	if (cmd == RTM_DELETE && error == 0 && ((rt = nrt) != NULL)){
		rt_newaddrmsg(cmd, ifa, error, nrt);
		if (rt->rt_refcnt <= 0){
			rt->rt_refcnt++;
			rtfree(rt);
		}
	}
	LOG_INFO("nrt:%x", (u32)nrt);

	if (cmd == RTM_ADD && error == 0 && ((rt = nrt) != NULL)){
		rt->rt_refcnt--;

		LOG_INFO("rt->ifa:%x, ifa:%x", (u32)rt->rt_ifa, (u32)ifa);
		if (rt->rt_ifa != ifa){
			LOG_INFO("wrong ifa");
			if(rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt, NULL);
			IFAFREE(rt->rt_ifa);
			rt->rt_ifa = ifa;
			rt->rt_ifp = ifa->ifa_ifp;
			ifa->ifa_refcnt++;
			if (ifa->ifa_rtrequest)
				ifa->ifa_rtrequest(RTM_ADD, rt, NULL);
		}
		rt_newaddrmsg(cmd, ifa, error, nrt);
	}
	LOG_INFO("end.error:%d", -error);
	return error;
}







