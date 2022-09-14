/*
 * if.c
 *
 *  Created on: Jun 14, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/net/if.h>
#include <kernel/types.h>
#include <kernel/net/if_dl.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/timer.h>
#include <kernel/net/radix.h>
#include <kernel/sys/sockio.h>
#include <kernel/sys/domain.h>
#include <kernel/sys/protosw.h>
#include <kernel/net/route.h>
#include <kernel/kthread.h>

struct ifnet *ifnet;
struct ifaddr **ifnet_addrs;
int if_index;


void
link_rtrequest(int cmd, struct rtentry *rt, struct sockaddr *sa);

int if_attach(struct ifnet *ifp)
{
	u32	socksize, ifasize;
	s32	namelen, unitlen, masklen;

	s8	workbuf[12], *unitname;
	struct ifnet **p = &ifnet;
	struct sockaddr_dl	*sdl;
	struct ifaddr *ifa;
	static s32	if_indexlim = 8;

	LOG_INFO("in");
	while (*p)
		p = &((*p)->if_next);

	*p = ifp;
	ifp->if_index = ++if_index;
	//resize the global ifaddr buffer
	if (ifnet_addrs == 0 || if_index >= if_indexlim){
		u32 n = (if_indexlim <<= 1) * sizeof(ifa);
		struct ifaddr **q = (struct ifaddr ** )kzalloc(n);
		if (ifnet_addrs){
			memcpy((void *)q, (void *)ifnet_addrs, n/2);
			kfree(ifnet_addrs);
		}

		ifnet_addrs = q;
	}

	sprintf(workbuf, "%d", (int)ifp->if_unit);
	unitname = workbuf;
	namelen = strlen(ifp->if_name);
	unitlen = strlen(unitname);

#define _offsetof(t, m)	((int)((caddr_t)&((t*)0)->m))

	masklen = _offsetof(struct sockaddr_dl, sdl_data[0]) + unitlen + namelen;

	socksize = masklen + ifp->if_addrlen;

#define ROUNDUP(a)	(1 + (((a) - 1) | (sizeof(u32) -1)))

	socksize = ROUNDUP(socksize);
	if (socksize < (sizeof (*sdl)))
		socksize = sizeof(*sdl);

	ifasize = sizeof(*ifa) + 2 * socksize;

	if ((ifa = (struct ifaddr *)kzalloc(ifasize))){
		sdl = (struct sockaddr_dl *)(ifa+1);
		sdl->sdl_len = socksize;
		sdl->sdl_family = AF_LINK;
		memcpy((void *)sdl->sdl_data, (void *)ifp->if_name, namelen);
		memcpy((void *)((caddr_t)(sdl->sdl_data)+namelen), (void *)unitname, unitlen);
		sdl->sdl_nlen = (namelen += unitlen);
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = ifp->if_type;
		sdl->sdl_alen = ifp->if_addrlen;

		ifnet_addrs[if_index -1] = ifa;
		ifa->ifa_ifp = ifp;
		ifa->ifa_next = ifp->if_addrlist;
		ifa->ifa_rtrequest = link_rtrequest;
		ifp->if_addrlist = ifa;
		ifa->ifa_addr = (struct sockaddr *)sdl;

		//set link layer address mask
		sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
		ifa->ifa_netmask = (struct sockaddr *)sdl;
		sdl->sdl_len = masklen;

		while (namelen != 0)
			sdl->sdl_data[--namelen] = 0xFF;
	}
	LOG_INFO("end");
	return 0;
}





#define Equal(a,b)	(strcmp((a), (b)) == 0)

struct ifaddr *
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	char *cp, *cp2, *cp3;
	char *cplim;
	struct ifaddr *ifa_maybe = 0;
	u32 af = addr->sa_family;

	if (af >= AF_MAX)
		return (0);
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		ifa_maybe = ifa;
		if (ifa->ifa_netmask == 0) {
			if (Equal((char *)addr, (char *)ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr && Equal((char *)addr, (char *)ifa->ifa_dstaddr)))
				return (ifa);
			continue;
		}
		cp = addr->sa_data;
		cp2 = ifa->ifa_addr->sa_data;
		cp3 = ifa->ifa_netmask->sa_data;
		cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
		for (; cp3 < cplim; cp3++)
			if ((*cp++ ^ *cp2++) & *cp3)
				break;
		if (cp3 == cplim)
			return (ifa);
	}
	return (ifa_maybe);
}




struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != addr->sa_family)
			continue;
		if (Equal((caddr_t)addr, (caddr_t)ifa->ifa_addr))
			return (ifa);
		if ((ifp->if_flags & IFF_BROADCAST) && ifa->ifa_broadaddr &&
				Equal((caddr_t)ifa->ifa_broadaddr, (caddr_t)addr))
			return (ifa);
	}
	return ((struct ifaddr *)0);
}


struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	LOG_DEBG("in");
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    if (ifp->if_flags & IFF_POINTOPOINT)
		for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family != addr->sa_family ||
			    ifa->ifa_dstaddr == NULL)
				continue;
			if (Equal((caddr_t)addr, (caddr_t)ifa->ifa_dstaddr))
				return (ifa);
	}
	LOG_DEBG("end");
	return ((struct ifaddr *)0);
}


/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = (struct ifaddr *) 0;
	u32 af = addr->sa_family;
	char *addr_data = addr->sa_data, *cplim;

	LOG_DEBG("in");
	if (af == AF_LINK) {
	    struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
	    if (sdl->sdl_index && sdl->sdl_index <= if_index)
		return (ifnet_addrs[sdl->sdl_index - 1]);
	}
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
	    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
	    	LOG_DEBG("ifa:%x", ifa);
			char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != af || ifa->ifa_netmask == 0)
				next: continue;
			cp = addr_data;
			cp2 = ifa->ifa_addr->sa_data;
			cp3 = ifa->ifa_netmask->sa_data;
			cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
			while (cp3 < cplim)
				if ((*cp++ ^ *cp2++) & *cp3++)
					goto next;
			if (ifa_maybe == 0 || rn_refines((caddr_t)ifa->ifa_netmask,(caddr_t)ifa_maybe->ifa_netmask))
				ifa_maybe = ifa;
		}
	LOG_DEBG("end");
	return (ifa_maybe);
}

void
if_slowtimo(void *arg)
{
	struct ifnet *ifp;
	//LOG_INFO("if slow time out");
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp->if_unit);
	}

}

void
ifinit()
{
	struct ifnet *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_snd.ifq_maxlen == 0)
			ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	register_timer(HZ / IFNET_SLOWHZ, TIMER_FLAG_PERIODIC|TIMER_FLAG_ENABLED, if_slowtimo, 0);
}

int
ifioctl(struct socket *so, u32 cmd, caddr_t data)
{
	struct ifnet *ifp;
	struct ifreq *ifr;

	LOG_INFO("in");

	switch (cmd) {
	case SIOCGIFCONF:
	case OSIOCGIFCONF:
		return (ifconf(cmd, data));
	}

	ifr = (struct ifreq *)data;
	ifp = ifunit(ifr->ifr_name);
	if (ifp == NULL) {
		LOG_ERROR("cannot find ifp for ifname:%s", ifr->ifr_name);
		return (-ENXIO);
	}

	LOG_INFO("cmd:%x, SIOCGIFFLAGS:%x", cmd, SIOCGIFFLAGS);
	switch (cmd) {
	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) && (ifr->ifr_flags & IFF_UP) == 0) {
			if_down(ifp);
		}
		if ((ifr->ifr_flags & IFF_UP) && (ifp->if_flags & IFF_UP) == 0) {
			if_up(ifp);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) | (ifr->ifr_flags &~ IFF_CANTCHANGE);
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		break;
	case SIOCSIFMETRIC:
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_ioctl == NULL)
			return (-EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));

	default:
		if (so->so_proto == 0)
			return (-EOPNOTSUPP);
		LOG_INFO("ifp:%x", (u32)ifp);
		return ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL, cmd, data, ifp));
	}
	return (0);
}

int
ifconf(int cmd, caddr_t data)
{
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp = ifnet;
	struct ifaddr *ifa;
	char *cp, *ep;
	struct ifreq ifr, *ifrp;
	int space = ifc->ifc_len, error = 0;


	LOG_INFO("in");
	ifrp = ifc->ifc_req;
	ep = ifr.ifr_name + sizeof (ifr.ifr_name) - 2;
	for (; space > sizeof (ifr) && ifp; ifp = ifp->if_next) {
		strncpy(ifr.ifr_name, ifp->if_name, sizeof (ifr.ifr_name) - 2);
		for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
			continue;
		*cp++ = '0' + ifp->if_unit; *cp = '\0';
		LOG_INFO("ifname:%s", ifr.ifr_name);
		if ((ifa = ifp->if_addrlist) == NULL) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			memcpy((void *)ifrp, (void *)&ifr, sizeof (ifr));
			LOG_INFO("ifrp_name:%s", ifrp->ifr_ifru.ifru_addr);
			space -= sizeof (ifr), ifrp++;
		} else
		    for ( ; space > sizeof (ifr) && ifa; ifa = ifa->ifa_next) {
		    	struct sockaddr *sa = ifa->ifa_addr;
		    	LOG_INFO("sa:%s len:%d", sa->sa_data, sa->sa_len);
				if (sa->sa_len <= sizeof(*sa)) {
					ifr.ifr_addr = *sa;
					memcpy((void *)ifrp, (void *)&ifr, sizeof (ifr));
					ifrp++;
				} else {
					space -= sa->sa_len - sizeof(*sa);
					if (space < sizeof (ifr))
						break;
					memcpy((void *)ifrp, (void *)&ifr, sizeof (ifr.ifr_name));
					memcpy((void *)&ifrp->ifr_addr, (void *)sa, sizeof(*sa));
					ifrp = (struct ifreq *)(sa->sa_len + (caddr_t)&ifrp->ifr_addr);
				}
				space -= sizeof (ifr);
		    }
	}
	ifc->ifc_len -= space;
	return (error);
}

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(char *name)
{
	char *cp;
	struct ifnet *ifp;
	int unit;
	unsigned len;
	char *ep, c;

	LOG_INFO("ifname:%s", name);
	for (cp = name; cp < name + IFNAMSIZ && *cp; cp++)
		if (*cp >= '0' && *cp <= '9')
			break;
	if (*cp == '\0' || cp == name + IFNAMSIZ)
		return NULL;
	/*
	 * Save first char of unit, and pointer to it,
	 * so we can put a null there to avoid matching
	 * initial substrings of interface names.
	 */
	len = cp - name + 1;
	c = *cp;
	ep = cp;
	for (unit = 0; *cp >= '0' && *cp <= '9'; )
		unit = unit * 10 + *cp++ - '0';
	*ep = 0;
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if (memcmp(ifp->if_name, name, len))
			continue;
		if (unit == ifp->if_unit)
			break;
	}
	*ep = c;
	return (ifp);
}

/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifqueue *ifq)
{
	struct mbuf *m, *n;

	n = ifq->ifq_head;
	while ((m = n) != NULL) {
		n = m->m_act;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
}


void
if_down(struct ifnet *ifp)
{
	struct ifaddr *ifa;

	ifp->if_flags &= ~IFF_UP;
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	if_qflush(&ifp->if_snd);
	rt_ifmsg(ifp);
}


void
if_up(struct ifnet *ifp)
{
	ifp->if_flags |= IFF_UP;
	rt_ifmsg(ifp);
}




void
link_rtrequest(int cmd, struct rtentry *rt, struct sockaddr *sa)
{
	struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	if ((ifa = ifaof_ifpforaddr(dst, ifp)) != NULL) {
		IFAFREE(rt->rt_ifa);
		rt->rt_ifa = ifa;
		ifa->ifa_refcnt++;
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, sa);
	}
}












