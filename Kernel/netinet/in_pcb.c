/*
 * in_pcb.c
 *
 *  Created on: Aug 19, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/malloc.h>
#include <kernel/net/route.h>
#include <string.h>
#include <kernel/mbuf.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/socketval.h>
#include <kernel/kthread.h>
#include <kernel/sys/socket.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/net/if.h>

#define MALLOC(p, t, s) p = (t)kmalloc((s))
#define FREE(p)	kfree((p));

struct	in_addr zeroin_addr;
extern struct 	in_ifaddr *in_ifaddr;

int in_pcballoc(struct socket *so, struct inpcb *head)
{
	struct inpcb *inp;
	LOG_INFO("in");
	MALLOC(inp, struct inpcb *, sizeof(*inp));
	if (inp == NULL)
		return -ENOBUFS;
	Bzero(inp, sizeof(*inp));
	inp->inp_next = inp->inp_prev = inp;
	inp->inp_head = head;
	inp->inp_socket = so;
	insque(inp, head);
	so->so_pcb = (caddr_t)inp;
	LOG_INFO("end");
	return 0;

}

int in_pcbdetach(struct inpcb *inp)
{
	LOG_INFO("in");

	struct socket *so = inp->inp_socket;
	so->so_pcb = NULL;
	sofree(so);
	if (inp->inp_options)
		m_free(inp->inp_options);
	if (inp->inp_route.ro_rt)
		rtfree(inp->inp_route.ro_rt);
	LOG_INFO("free ipmoptions");
	ip_freemoptions(inp->inp_moptions);
	remque(inp);
	LOG_INFO("free inp:%x", (u32)inp);
	if (inp)
		FREE(inp);
	LOG_INFO("end");
	return 0;
}


struct inpcb *
in_pcblookup(struct inpcb *head, struct in_addr faddr,
		u32 fport_arg, struct in_addr laddr, u32 lport_arg,
		int flags)
{
	struct inpcb *inp, *match = NULL;
	int matchwild = 3, wildcard;
	u16	fport = fport_arg, lport = lport_arg;

	//LOG_DEBG("in");
	for (inp = head->inp_next; inp != head; inp = inp->inp_next){
		if (inp->inp_lport != lport)
			continue;

		wildcard = 0;

		if (inp->inp_laddr.s_addr != INADDR_ANY){
			if (laddr.s_addr == INADDR_ANY)
				wildcard++;
			else if (inp->inp_laddr.s_addr != laddr.s_addr)
				continue;
		} else {
			if (laddr.s_addr != INADDR_ANY)
				wildcard++;
		}

		if (inp->inp_faddr.s_addr != INADDR_ANY){
			if (faddr.s_addr == INADDR_ANY)
				wildcard++;
			else if (inp->inp_faddr.s_addr != faddr.s_addr ||
					inp->inp_fport != fport	)
				continue;
		} else {
			if (faddr.s_addr != INADDR_ANY)
				wildcard++;
		}


		if (wildcard && (flags & INPLOOKUP_WILDCARD) == 0)
			continue;

		if (wildcard < matchwild){
			match = inp;
			matchwild = wildcard;
			if (matchwild == 0)
				break;
		}
	}
	//LOG_DEBG("end");
	return match;
}

int in_pcbbind(struct inpcb *inp, struct mbuf *nam)
{
	struct socket *so = inp->inp_socket;
	struct inpcb *head = inp->inp_head;
	struct sockaddr_in *sin;
	//kprocess_t *p = get_current_process();
	u16	lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	//int error;

	if (in_ifaddr == 0)
		return -EADDRNOTAVAIL;

	if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
		return -EINVAL;

	if ((so->so_options & (SO_REUSEADDR | SO_REUSEPORT)) == 0 &&
			((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
					(so->so_options & SO_ACCEPTCONN) == 0))
		wild = INPLOOKUP_WILDCARD;


	if (nam){
		sin = mtod(nam, struct sockaddr_in *);
		if (nam->m_len != sizeof(*sin))
			return -EINVAL;

		if (sin->sin_family != AF_INET)
			return -EAFNOSUPPORT;

		lport = sin->sin_port;
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))){
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY){
			sin->sin_port = 0;
			if (ifa_ifwithaddr((struct sockaddr *)sin) == 0)
				return -EADDRNOTAVAIL;
		}
	}

	if (lport){
		struct inpcb *t;
		/*
		 * check port < 1024, should be root use
		 */

		t = in_pcblookup(head, zeroin_addr, 0, sin->sin_addr, lport, wild);
		if (t && (reuseport & t->inp_socket->so_options) == 0)
			return -EADDRINUSE;
	}

	inp->inp_laddr = sin->sin_addr;

	if (lport == 0)
		do {
			if (head->inp_lport++ < IPPORT_RESERVED || head->inp_lport > IPPORT_USERRESERVED)
				lport = htons(head->inp_lport);
			//port in head stored as host byte order, but in other pcb, which is network byte order
		} while (in_pcblookup(head, zeroin_addr, 0, inp->inp_laddr, lport, wild));

	inp->inp_lport = lport;
	return 0;
}

int in_pcbconnect(struct inpcb *inp, struct mbuf *nam)
{
	struct in_ifaddr *ia;
	struct sockaddr_in *ifaddr = NULL;
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

	LOG_DEBG("in");
	if (nam->m_len != sizeof(*sin))
		return -EINVAL;
	if (sin->sin_family != AF_INET)
		return -EAFNOSUPPORT;
	if (sin->sin_port == 0)
		return -EADDRNOTAVAIL;
	if (in_ifaddr){
#define satosin(sa)	((struct sockaddr_in *)(sa))
#define sintosa(sin)	((struct sockaddr *)(sin))
#define ifatoia(ia)	((struct in_ifaddr *)(ia))
		if (sin->sin_addr.s_addr == INADDR_ANY)
			sin->sin_addr = IA_SIN(in_ifaddr)->sin_addr;
		else if (sin->sin_addr.s_addr == (u32)INADDR_BROADCAST &&
				(in_ifaddr->ia_ifp->if_flags & IFF_BROADCAST)) {
			sin->sin_addr = satosin(&in_ifaddr->ia_broadaddr)->sin_addr;
			LOG_INFO("broadcast address detected, change dst to if broadcast addr:%x", sin->sin_addr.s_addr);

		}

	}

	if (inp->inp_laddr.s_addr == INADDR_ANY){
		struct route *ro;
		ia = (struct in_ifaddr *)0;
		ro = &inp->inp_route;
		if (ro->ro_rt &&
				(satosin(&ro->ro_dst)->sin_addr.s_addr != sin->sin_addr.s_addr ||
						(inp->inp_socket->so_options & SO_DONTROUTE))){
			RTFREE(ro->ro_rt);
			ro->ro_rt = NULL;
		}

		if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 &&
				(ro->ro_rt == (struct rtentry *)0 ||
						ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sin->sin_addr;
			rtalloc(ro);
		}
		if (ro->ro_rt && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
			ia = ifatoia((ro->ro_rt->rt_ifa));
		LOG_INFO("ia:%x", (u32)ia);

		if (ia == NULL){
			u16	fport = sin->sin_port;
			sin->sin_port = 0;
			ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
			if (ia == NULL)
				ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
			sin->sin_port = fport;
			if (ia == NULL)
				ia = in_ifaddr;
			if (ia == NULL) {
				LOG_DEBG("ia == NULL");
				return -EADDRNOTAVAIL;
			}

		}

		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
				inp->inp_moptions != NULL){
			struct ip_moptions *imo;
			struct ifnet *ifp;
			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL){
				ifp = imo->imo_multicast_ifp;
				for (ia = in_ifaddr; ia; ia = ia->ia_next)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == NULL)
					return -EADDRNOTAVAIL;
			}
		}
		ifaddr = (struct sockaddr_in *)&ia->ia_addr;
	}

	if (in_pcblookup(inp->inp_head, sin->sin_addr, sin->sin_port,
			inp->inp_laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr,
					inp->inp_lport, 0))
		return -EADDRINUSE;

	if (inp->inp_laddr.s_addr == INADDR_ANY){
		if (inp->inp_lport == 0)
			in_pcbbind(inp, NULL);
		inp->inp_laddr = ifaddr->sin_addr;
	}

	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	LOG_INFO("end, inp.laddr:%x inp.faddr:%x, inp route:%x", inp->inp_laddr, inp->inp_faddr, (u32)inp->inp_route.ro_rt);
	return 0;
}


void in_pcbdisconnect(struct inpcb *inp)
{
	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
	if (inp->inp_socket->so_state & SS_NOFDREF) {
		in_pcbdetach(inp);
	}

}

void in_setsockaddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in *sin;
	nam->m_len = sizeof(*sin);
	sin = mtod(nam, struct sockaddr_in *);
	Bzero((caddr_t)sin, sizeof(*sin));

	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
}

void in_setpeeraddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in *sin;
	nam->m_len = sizeof(*sin);
	sin = mtod(nam, struct sockaddr_in *);
	Bzero(sin, sizeof(*sin));

	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;

}

void in_rtchange(struct inpcb *inp, int error)
{
	if (inp->inp_route.ro_rt){
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = NULL;
	}
}

void in_pcbnotify(struct inpcb *head, struct sockaddr *dst,
		u32	fport_arg, struct in_addr laddr, u32 lport_arg,
		int cmd, void (*notify)(struct inpcb *, int))
{
	extern u8 inetctlerrmap[];
	struct inpcb *inp, *oinp;
	struct in_addr faddr;
	u16	fport = fport_arg, lport = lport_arg;
	int error;

	if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET)
		return;
	faddr = ((struct sockaddr_in *)dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD){
		fport = 0;
		lport = 0;
		laddr.s_addr = 0;
		if (cmd != PRC_HOSTDEAD)
			notify = in_rtchange;
	}

	error = inetctlerrmap[cmd];
	for (inp = head->inp_next; inp != head;){
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
				inp->inp_socket == NULL ||
				(lport && inp->inp_lport != lport) ||
				(laddr.s_addr && inp->inp_laddr.s_addr != laddr.s_addr) ||
				(fport && inp->inp_fport != fport)){
			inp = inp->inp_next;
			continue;
		}

		oinp = inp;
		inp = inp->inp_next;
		if (notify)
			(*notify)(oinp, error);
	}
}


void in_losing(struct inpcb *inp)
{
	struct rtentry *rt;
	struct rt_addrinfo info;

	if ((rt = inp->inp_route.ro_rt) != NULL){
		inp->inp_route.ro_rt = NULL;
		Bzero(&info, sizeof(info));
		info.rti_info[RTAX_DST] = (struct sockaddr *)&inp->inp_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);

		if (rt->rt_flags & RTF_DYNAMIC)
			rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,rt_mask(rt), rt->rt_flags, NULL);
		else
			rtfree(rt);
	}
}






