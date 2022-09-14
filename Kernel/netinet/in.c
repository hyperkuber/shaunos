/*
 * in.c
 *
 *  Created on: Aug 1, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/malloc.h>
#include <kernel/net/if.h>
#include <kernel/kernel.h>
#include <kernel/net/route.h>
#include <kernel/sys/sockio.h>
#include <kernel/net/if_ether.h>

extern struct 	in_ifaddr *in_ifaddr;



/*
 * Trim a mask in a sockaddr
 */
void
in_socktrim(struct sockaddr_in *ap)
{
	char *cplim = (char *) &ap->sin_addr;
	char *cp = (char *) (&ap->sin_addr + 1);

	ap->sin_len = 0;
	while (--cp >= cplim)
		if (*cp) {
			(ap)->sin_len = cp - (char *) (ap) + 1;
			break;
		}
}
/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	u32 i = ntohl(in.s_addr);
	u32 net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}
/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	u32 t;
	//struct in_ifaddr *ia;
	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return 0;
	t = ntohl(in.s_addr);
	LOG_INFO("t:%x, in.saddr:%x", t, in.s_addr);
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
//#define ia ((struct in_ifaddr *)ifa)
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ((struct in_ifaddr*)ifa)->ia_dstaddr.sin_addr.s_addr ||
		     in.s_addr == ((struct in_ifaddr*)ifa)->ia_netbroadcast.s_addr ||
		     t == ((struct in_ifaddr*)ifa)->ia_subnet || t == ((struct in_ifaddr*)ifa)->ia_net))
			    return 1;
	LOG_INFO("end.return 0");
	return (0);
//#undef ia
}


/*
 * Add an address to the list of IP multicast addresses for a given interface.
 */
struct in_multi *
in_addmulti(struct in_addr *ap, struct ifnet *ifp)
{
	struct in_multi *inm;
	struct ifreq ifr;
	struct in_ifaddr *ia;

	LOG_INFO("in");
	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);
	if (inm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		++inm->inm_refcount;
	}
	else {
		/*
		 * New address; allocate a new multicast record
		 * and link it into the interface's multicast list.
		 */
		inm = (struct in_multi *)kmalloc(sizeof(*inm));
		if (inm == NULL) {
			return (NULL);
		}
		inm->inm_addr = *ap;
		inm->inm_ifp = ifp;
		inm->inm_refcount = 1;
		IFP_TO_IA(ifp, ia);
		if (ia == NULL) {
			kfree(inm);
			return (NULL);
		}
		inm->inm_ia = ia;
		inm->inm_next = ia->ia_multiaddrs;
		ia->ia_multiaddrs = inm;
		/*
		 * Ask the network driver to update its multicast reception
		 * filter appropriately for the new address.
		 */
		((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET;
		((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr = *ap;
		if ((ifp->if_ioctl == NULL) ||
		    (*ifp->if_ioctl)(ifp, SIOCADDMULTI,(caddr_t)&ifr) != 0 ) {
			ia->ia_multiaddrs = inm->inm_next;
			kfree(inm);
			return (NULL);
		}
		/*
		 * Let IGMP know that we have joined a new IP multicast group.
		 */
		//igmp_joingroup(inm);
	}
	LOG_INFO("end.inm:%x",(u32)inm);
	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
in_delmulti(struct in_multi *inm)
{
	struct in_multi **p;
	struct ifreq ifr;


	if (--inm->inm_refcount == 0) {
		/*
		 * No remaining claims to this record; let IGMP know that
		 * we are leaving the multicast group.
		 */
		//TODO:igmp
		//igmp_leavegroup(inm);
		/*
		 * Unlink from list.
		 */
		for (p = &inm->inm_ia->ia_multiaddrs;
		     *p != inm;
		     p = &(*p)->inm_next)
			 continue;
		*p = (*p)->inm_next;
		/*
		 * Notify the network driver to update its multicast reception
		 * filter.
		 */
		((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
		((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr =
								inm->inm_addr;
		//TODO:ioctl
		//(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
		//					     (caddr_t)&ifr);
		kfree(inm);
	}

}
int in_control(struct socket *so, u32 cmd,caddr_t data, struct ifnet *ifp)
{

	struct ifreq *ifr = (struct ifreq *)data;
	struct in_ifaddr *ia = NULL;
	struct ifaddr *ifa;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostisnew, maskisnew;
	u32 i;

	LOG_INFO("in, ifp:%x", (u32)ifp	);
	if (ifp) {
		for (ia=in_ifaddr; ia; ia=ia->ia_next)
			if (ia->ia_ifp == ifp)
				break;
	}

	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sin_family == AF_INET)
			for (oia = ia; ia; ia = ia->ia_next) {
				if (ia->ia_ifp == ifp  &&
						ia->ia_addr.sin_addr.s_addr ==
								ifra->ifra_addr.sin_addr.s_addr)
			    break;
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (-EADDRNOTAVAIL);
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
//		if ((so->so_state & SS_PRIV) == 0)
//			return -EPERM;
		if (ifp == NULL)
			LOG_WARN("ifp == NULL");

		if (ia == NULL) {
			oia = (struct in_ifaddr *)kzalloc(sizeof(struct in_ifaddr));
			if (oia == NULL)
				return -ENOBUFS;
			if ((ia = in_ifaddr) != NULL) {
				for (; ia->ia_next; ia = ia->ia_next)
					continue;
				ia->ia_next = oia;
			} else
				in_ifaddr = oia;
			ia = oia;
			if ((ifa = ifp->if_addrlist) != NULL) {
				for (; ifa->ifa_next; ifa = ifa->ifa_next)
					continue;
				ifa->ifa_next = (struct ifaddr *)&ia->ia_ifa;
			} else
				ifp->if_addrlist = (struct ifaddr *)ia;
			ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ia->ia_ifa.ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ia->ia_sockmask.sin_len = 8;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
//			if (ifp != &loif)
//				in_interfaces++;

		}
		break;
	case SIOCSIFBRDADDR:
//		if ((so->so_state & SS_PRIV) == 0)
//			return -EPERM;
	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia == NULL)
			return -EADDRNOTAVAIL;
		break;
	}

	switch (cmd) {
	case SIOCGIFADDR:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (-EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (-EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_sockmask;
		break;

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (-EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *(struct sockaddr_in *)&ifr->ifr_dstaddr;
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			return (error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
					(struct sockaddr *)&ia->ia_dstaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		break;

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (-EINVAL);
		ia->ia_broadaddr = *(struct sockaddr_in *)&ifr->ifr_broadaddr;
		break;

	case SIOCSIFADDR:
		return (in_ifinit(ifp, ia,
			(struct sockaddr_in *) &ifr->ifr_addr, 1));

	case SIOCSIFNETMASK:
		i = ifra->ifra_addr.sin_addr.s_addr;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr = i);
		break;

	case SIOCAIFADDR:
		maskisnew = 0;
		hostisnew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostisnew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
						   ia->ia_addr.sin_addr.s_addr)
				hostisnew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_subnetmask =
				 ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskisnew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
			(ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskisnew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
			(hostisnew || maskisnew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if ((ifp->if_flags & IFF_BROADCAST) &&
			(ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		LOG_INFO("end. error:%d", -error);
		return (error);

	case SIOCDIFADDR:
		in_ifscrub(ifp, ia);
		if ((ifa = ifp->if_addrlist) == (struct ifaddr *)ia)
			ifp->if_addrlist = ifa->ifa_next;
		else {
			while (ifa->ifa_next &&
				   (ifa->ifa_next != (struct ifaddr *)ia))
					ifa = ifa->ifa_next;
			if (ifa->ifa_next)
				ifa->ifa_next = ((struct ifaddr *)ia)->ifa_next;
			else
				LOG_ERROR("Couldn't unlink inifaddr from ifp");
		}
		oia = ia;
		if (oia == (ia = in_ifaddr))
			in_ifaddr = ia->ia_next;
		else {
			while (ia->ia_next && (ia->ia_next != oia))
				ia = ia->ia_next;
			if (ia->ia_next)
				ia->ia_next = oia->ia_next;
			else
				LOG_ERROR("didn't unlink inifadr from list");
		}
		IFAFREE((&oia->ia_ifa));
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (-EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}

	return 0;
}

#ifndef SUBNETSARELOCAL
#define	SUBNETSARELOCAL	1
#endif
int subnetsarelocal = SUBNETSARELOCAL;
/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(struct in_addr in)
{
	u32 i = ntohl(in.s_addr);
	struct in_ifaddr *ia;

	if (subnetsarelocal) {
		for (ia = in_ifaddr; ia; ia = ia->ia_next)
			if ((i & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		for (ia = in_ifaddr; ia; ia = ia->ia_next)
			if ((i & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia)
{

	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
	else
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
	ia->ia_flags &= ~IFA_ROUTE;
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin, int scrub)
{
	u32 i = ntohl(sin->sin_addr.s_addr);
	struct sockaddr_in oldaddr;
	int flags = RTF_UP, error, ether_output();

	LOG_INFO("in");
	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		ia->ia_addr = oldaddr;
		return (error);
	}
	if (ifp->if_output == ether_output) { /* XXX: Another Kludge */
		ia->ia_ifa.ifa_rtrequest = arp_rtrequest;
		ia->ia_ifa.ifa_flags |= RTF_CLONING;
	}
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	LOG_INFO("i:%x", i);
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
			htonl(ia->ia_net | ~ ia->ia_netmask);
		LOG_INFO("ia broadaddr:%x subnet:%x mask:%x", ia->ia_broadaddr.sin_addr.s_addr, ia->ia_subnet, ia->ia_subnetmask);
		LOG_INFO("ia netbroadcast addr:%x", ia->ia_netbroadcast.s_addr);


	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}
	if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags)) == 0)
		ia->ia_flags |= IFA_ROUTE;
	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
		in_addmulti(&addr, ifp);
	}
	LOG_INFO("end.error:%d", -error);
	return (error);
}


