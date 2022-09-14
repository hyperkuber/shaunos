/*
 * if_ether.c
 *
 *  Created on: Aug 1, 2013
 *      Author: Shaun Yuan
 */


#include <kernel/net/if_ether.h>
#include <kernel/net/route.h>
#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <kernel/sys/endian.h>
#include <kernel/net/if_dl.h>
#include <kernel/sys/netinet/in_val.h>
#include <string.h>
#include <kernel/malloc.h>


/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)
#define SRP(s) ((struct sockaddr_inarp *)s)

extern struct 	in_ifaddr *in_ifaddr;
extern struct timespec xtime;
#define CURRENT_TIME_SEC (xtime.ts_sec)
/* timer values */
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */

#define	rt_expire rt_rmx.rmx_expire
struct	llinfo_arp llinfo_arp = {&llinfo_arp, &llinfo_arp};
struct	ifqueue arpintrq = {0, 0, 0, 50};
int	arp_inuse, arp_allocated, arp_intimer;
int	arp_maxtries = 5;
int	useloopback = 1;	/* use loopback interface for local traffic */
int	arpinit_done = 0;

extern u8 etherbroadcastaddr[6];



static void
arprequest(struct arpcom *ac, u32 *sip, u32 *tip, u8 *enaddr)
{
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct sockaddr sa;

	LOG_INFO("in");
	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL) {
		LOG_ERROR("no buf");
		return;
	}
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));

	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	Bzero((caddr_t)ea, sizeof(*ea));

	Bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->dest_mac, sizeof(eh->dest_mac));

	eh->eth_type = ETHERTYPE_ARP;
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);
	ea->arp_pln = sizeof(ea->arp_spa);
	ea->arp_op = htons(ARPOP_REQUEST);
	Bcopy((caddr_t)enaddr, (caddr_t)ea->arp_sha, sizeof(ea->arp_sha));
	Bcopy((caddr_t)sip, (caddr_t)(ea->arp_spa), sizeof(ea->arp_spa));
	Bcopy((caddr_t)tip, (caddr_t)(ea->arp_tpa), sizeof(ea->arp_tpa));

	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);

	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, NULL);
	LOG_INFO("end");

}



void arpwhohas (struct arpcom *ac, struct in_addr *addr)
{
	LOG_INFO("arp who has addr:%x tell:%d", addr->s_addr,  ac->ac_ipaddr.s_addr);
	arprequest(ac, (u32 *)&ac->ac_ipaddr.s_addr, (u32 *)&addr->s_addr, ac->ac_enaddr);
}

static struct llinfo_arp *
arplookup(u32 addr, int create, int proxy)
{
	struct rtentry *rt;
	static struct sockaddr_inarp sin = {sizeof(sin), AF_INET};
	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;

	LOG_INFO("in");
	rt = rtalloc1((struct sockaddr *)&sin, create);
	if (rt == NULL)
		return NULL;
	rt->rt_refcnt--;
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
			rt->rt_gateway->sa_family != AF_LINK) {
		if (create)
			LOG_INFO("arpnew failed");
		LOG_INFO("rt gateway or no link info, rt flags:%x", rt->rt_flags);
		return NULL;
	}
	LOG_INFO("end");
	return ((struct llinfo_arp*)rt->rt_llinfo);
}


static void
in_arpinput(struct mbuf *m)
{
	struct ether_arp *ea;
	struct arpcom *ac = (struct arpcom *)m->m_pkthdr.rcvif;
	struct ether_header *eh;
	struct llinfo_arp *la = NULL;
	struct rtentry *rt;
	struct in_ifaddr *ia, *maybe_ia = NULL;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;

	LOG_INFO("in");
	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	Bcopy((caddr_t)ea->arp_spa, (caddr_t)&isaddr, sizeof(isaddr));
	Bcopy((caddr_t)ea->arp_tpa, (caddr_t)&itaddr, sizeof(itaddr));

	for(ia = in_ifaddr; ia; ia=ia->ia_next){
		if(ia->ia_ifp == &ac->ac_if){
			maybe_ia = ia;
			if ((itaddr.s_addr == ia->ia_addr.sin_addr.s_addr) ||
					(isaddr.s_addr == ia->ia_addr.sin_addr.s_addr))
				break;
		}
	}

	if(maybe_ia == NULL) {
		LOG_INFO("arp not for us, out");
		goto out;
	}


	myaddr = ia ? ia->ia_addr.sin_addr : maybe_ia->ia_addr.sin_addr;

	if (!Bcmp((caddr_t)ea->arp_sha, (caddr_t)ac->ac_enaddr, sizeof(ea->arp_sha)))
		goto out;
	if (!Bcmp((caddr_t)ea->arp_sha, (caddr_t)etherbroadcastaddr, sizeof(ea->arp_sha))){
		LOG_ERROR("ether address is broadcast for IP address");
		goto out;
	}

	if (isaddr.s_addr == myaddr.s_addr){
		LOG_ERROR("duplicate Ip address");
		BUG();
		itaddr = myaddr;
		goto reply;
	}

	la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))){
		if (sdl->sdl_alen &&
				Bcmp((caddr_t)ea->arp_sha, LLADDR(sdl), sdl->sdl_alen) != 0)
			LOG_INFO("arp info overwriteten");	//machine may restart with another ether card
		Bcopy((caddr_t)ea->arp_sha, LLADDR(sdl), sdl->sdl_alen = sizeof(ea->arp_sha));
		if (rt->rt_expire)
			rt->rt_expire = CURRENT_TIME_SEC + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		if (la->la_hold){
			(*ac->ac_if.if_output)(&ac->ac_if, la->la_hold, rt_key(rt), rt);
			la->la_hold = NULL;
		}
	}
	reply:
	if (op != ARPOP_REQUEST){
		out:
		m_freem(m);
		return;
	}
	LOG_INFO("arp for us, response it, myaddr:%x", myaddr.s_addr);
	//ok, now, this arp packet is for us, response it.
	if (itaddr.s_addr == myaddr.s_addr){
		Bcopy(ea->arp_sha, ea->arp_tha, sizeof(ea->arp_sha));
		Bcopy(ac->ac_enaddr, ea->arp_sha, sizeof(ea->arp_sha));
	} else {
		la = arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (la == NULL) {
			LOG_INFO("la == NULL");
			goto out;
		}

		rt = la->la_rt;
		Bcopy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
	}

	Bcopy(ea->arp_spa, ea->arp_tpa, sizeof(ea->arp_spa));
	Bcopy(&itaddr, ea->arp_spa, sizeof(ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP);
	eh = (struct ether_header *)sa.sa_data;
	Bcopy(ea->arp_tha, eh->dest_mac, sizeof(eh->dest_mac));
	eh->eth_type = ETHERTYPE_ARP;
	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, NULL);
	LOG_INFO("end");
	return;

}






int arpresolve(struct arpcom *ac, struct rtentry *rt,
		struct mbuf *m, struct sockaddr *dst, u8 *desten)
{
	struct llinfo_arp *la;
	struct sockaddr_dl *sdl;

	LOG_INFO("in,dest:%x", ((struct sockaddr_in *)(dst))->sin_addr.s_addr);
	if (m->m_flags & M_BCAST){
		Bcopy(etherbroadcastaddr, desten, sizeof(etherbroadcastaddr));
		return 1;
	}

	if (m->m_flags & M_MCAST){
		ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return 1;
	}

	if (rt)
		la = (struct llinfo_arp *)rt->rt_llinfo;
	else {
		if ((la = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0)) != NULL)
			rt = la->la_rt;
	}

	if (la == NULL || rt == NULL){
		LOG_INFO("can not allocate llinfo");
		m_free(m);
		return 0;
	}

	sdl = SDL(rt->rt_gateway);
	if ((rt->rt_expire == 0 || rt->rt_expire > CURRENT_TIME_SEC) &&
			sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0){
		Bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
		return 1;
	}

	if (la->la_hold)
		m_freem(la->la_hold);	// here, we may lost some packets
	la->la_hold = m;
	if (rt->rt_expire){
		rt->rt_flags &= ~ RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != CURRENT_TIME_SEC){
			rt->rt_expire = CURRENT_TIME_SEC;
			if(la->la_asked++ < arp_maxtries)
				arpwhohas(ac, &(SIN(dst)->sin_addr));
			else {	//to avoid arp flooding
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
			}
		}
	}
	return 0;
}

void arp_rtrequest(int req, struct rtentry *rt, struct sockaddr *sa)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	if (!arpinit_done){
		arpinit_done = 1;
		//timeout(arptimer, NULL, hz);
	}

	LOG_INFO("in");
	if (rt->rt_flags & RTF_GATEWAY)
		return ;

	switch(req){
	case RTM_ADD:
		if ((rt->rt_flags & RTF_HOST) == 0 &&
				SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING){
			rt_setgate(rt, rt_key(rt), (struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = CURRENT_TIME_SEC;
			break;
		}
		if(rt->rt_flags & RTF_ANNOUNCE)
			arprequest((struct arpcom*)rt->rt_ifp, (u32 *)&(SIN(rt_key(rt))->sin_addr.s_addr),
					(u32 *)&(SIN(rt_key(rt))->sin_addr.s_addr),
					(u8*)LLADDR(SDL(gate)));
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
				gate->sa_len < sizeof(null_sdl)){
			LOG_INFO("bad gateway value");
			break;
		}

		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != NULL)
			break;

		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == NULL){
			LOG_INFO("malloc failed");
			break;
		}

		arp_inuse++;
		arp_allocated++;
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		insque(la, &llinfo_arp);

		if (SIN(rt_key(rt))->sin_addr.s_addr ==
				(IA_SIN(rt->rt_ifa))->sin_addr.s_addr){
			rt->rt_expire = 0;
			Bcopy(((struct arpcom *)rt->rt_ifp)->ac_enaddr, LLADDR(SDL(gate)), SDL(gate)->sdl_alen = 6);
			if (useloopback)
				//rt->rt_ifp = &loif;
				;
		}

		break;
	case RTM_DELETE:
		if (la == NULL)
			break;
		arp_inuse--;
		remque(la);
		rt->rt_llinfo = NULL;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			m_freem(la->la_hold);
		Free(la);

	}
}



/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr()
{
	struct mbuf *m;
	struct arphdr *ar;
	int x;
	LOG_INFO("in");
	while (arpintrq.ifq_head){

		local_irq_save(&x);
		IF_DEQUEUE(&arpintrq, m);
		local_irq_restore(x);
		if (m==NULL || (m->m_flags & M_PKTHDR) == 0)
			LOG_INFO("invalid mbuf");
		if (m->m_len >= sizeof(struct arphdr) &&
				(ar = mtod(m, struct arphdr *)) &&
				ntohs(ar->ar_hrd) == ARPHRD_ETHER &&
				m->m_len >= sizeof(struct arphdr) + 2*ar->ar_hln + 2*ar->ar_pln){
			switch(ntohs(ar->ar_pro)){
			case ETHERTYPE_IP:
			case ETHERTYPE_IPTRAILERS:
				in_arpinput(m);
				continue;
			}
		}

		m_freem(m);
	}
}








