/*
 * if_ethersubr.c
 *
 *  Created on: Jun 21, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/net/if_ethersubr.h>
#include <kernel/time.h>
#include <asm/io.h>
#include <kernel/net/netisr.h>
#include <kernel/mbuf.h>
#include <string.h>
#include <kernel/time.h>
#include <kernel/net/route.h>
#include <kernel/net/if_ether.h>
#include <kernel/sys/endian.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/malloc.h>


extern struct timespec xtime;
#define CURRENT_TIME_SEC (xtime.ts_sec)

extern u8 etherbroadcastaddr[6];

extern struct ifqueue	ipintrq;
struct ifqueue	arpintrq;



int
ether_input(struct ifnet *ifp, struct ether_header *et, struct mbuf *m)
{
	struct ifqueue *inq;

//	struct arpcom *ac = (struct arpcom *)ifp;

	LOG_INFO("ether packet in, dst mac:%x-%x-%x-%x-%x-%x src mac:%x-%x-%x-%x-%x-%x",
			et->dest_mac[0], et->dest_mac[1],et->dest_mac[2],et->dest_mac[3],et->dest_mac[4],et->dest_mac[5],
			et->src_mac[0],et->src_mac[1],et->src_mac[2],et->src_mac[3],et->src_mac[4],et->src_mac[5]);

	if ((ifp->if_flags & IFF_UP) == 0){
		m_freem(m);
		return 0;
	}

	ifp->if_lastchange.tv_sec = CURRENT_TIME_SEC;
	ifp->if_ibytes += (m->m_pkthdr.len + sizeof(*et));

	if(memcmp((void *)etherbroadcastaddr,
			(void *)et->dest_mac, sizeof(etherbroadcastaddr)) == 0){
		m->m_flags |= M_BCAST;
	} else if (et->dest_mac[0] & 0x01){
		m->m_flags |= M_MCAST;
	}

	if (m->m_flags & (M_BCAST | M_MCAST))
		ifp->if_imcasts++;

	switch (et->eth_type){
	case ETHERTYPE_IP:
		LOG_INFO("ether type IP:%x", et->eth_type);
		inq = &ipintrq;
		schednetisr(NETISR_IP);
		break;
	case ETHERTYPE_ARP:
		LOG_INFO("ether type ARP:%x", et->eth_type);
		inq = &arpintrq;
		schednetisr(NETISR_ARP);
		break;
	default:
		LOG_INFO("UnKnow ether type:%x", et->eth_type);
		m_freem(m);
		return -1;
	}


	if (IF_QFULL(inq)){
		LOG_INFO("inq full");
		IF_QDROP(inq);
		m_freem(m);
	} else {
		IF_ENQUEUE(inq, m);
	}

	LOG_INFO("end, mbuf_allocated:%d mcl_allocated:%d", mbuf_allocated, mcl_allocated);
	return 0;
}


int ether_output(struct ifnet *ifp, struct mbuf * m0,
		struct sockaddr *dst, struct rtentry *rt0)
{
	short type;
	int retval = 0;
	u8	edst[6];
	struct mbuf *m = m0;
	struct rtentry *rt;
	struct ether_header *eh;
	struct mbuf *mcopy = NULL;
	int off, len = m->m_pkthdr.len;
	struct arpcom *ac = (struct arpcom *)ifp;
	int x;
	LOG_INFO("in, dst:%x", ((struct sockaddr_in *)dst)->sin_addr.s_addr);
	if((ifp->if_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING)){
		LOG_INFO("interface down");
		retval = -ENETDOWN;
		goto END;
	}

	ti_get_timeval(&ifp->if_lastchange);
	//if rt0 != NULL
	if ((rt = rt0) != NULL){
		if((rt->rt_flags & RTF_UP) == 0){
			if((rt0 = rt = rtalloc1(dst, 1)) != NULL)
				rt->rt_refcnt--;
			else{
				retval = -EHOSTUNREACH;
				goto END;
			}

			if (rt->rt_flags & RTF_GATEWAY){
				if (rt->rt_gwroute == 0)
					goto lookup;
				if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0){
					rtfree(rt);
					rt = rt0;
					lookup:
					rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1);
					if ((rt = rt->rt_gwroute) == 0){
						retval = -EHOSTUNREACH;
						goto END;
					}
				}
			}

			if (rt->rt_flags & RTF_REJECT){
				if (rt->rt_rmx.rmx_expire == 0 || CURRENT_TIME_SEC <
						rt->rt_rmx.rmx_expire){
					retval = (rt == rt0 ? -EHOSTDOWN : -EHOSTUNREACH);
					goto END;
				}
			}
		}
	}

	switch (dst->sa_family) {
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst)) {
			LOG_INFO("arpresolve return 0");
			return 0;
		}

		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX)){
			mcopy = m_copy(m, 0, M_COPYALL);
		}

		off = m->m_pkthdr.len - m->m_len;
		type = htons(ETHERTYPE_IP);

		break;
	case AF_ISO:
		break;
	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
		memcpy((void *)edst, (void *)eh->dest_mac, sizeof(edst));
		type = htons(eh->eth_type);
		LOG_INFO("edst:%x.%x.%x.%x.%x.%x", edst[0],edst[1],edst[2],edst[3],edst[4],edst[5]);
		break;
	default:
		LOG_ERROR("Unknow family:%x", dst->sa_family);
		retval = -EAFNOSUPPORT;
		goto END;

	}

	if (mcopy){

		//send broadcast packet to loop interface
		//looutput(ifp, mcopy, dst, rt0);
	}
	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL){
		retval = -ENOBUFS;
		goto END;
	}

	//build ether header
	eh = mtod(m, struct ether_header *);
	eh->eth_type = type;
	//memcpy((void *)eh->eth_type, (void *)type, sizeof(type));
	memcpy((void *)eh->dest_mac, (void *)edst, ETHER_ADDR_LEN);
	LOG_INFO("dstmac:%x.%x.%x.%x.%x.%x", eh->dest_mac[0],eh->dest_mac[1],eh->dest_mac[2],eh->dest_mac[3],eh->dest_mac[4],eh->dest_mac[5]);
	//arguments on which src mac we should use is useless
	memcpy((void *)eh->src_mac, (void *)ac->ac_enaddr, ETHER_ADDR_LEN);
	LOG_INFO("srcmac:%x.%x.%x.%x.%x.%x", eh->src_mac[0],eh->src_mac[1],eh->src_mac[2],eh->src_mac[3],eh->src_mac[4],eh->src_mac[5]);
	local_irq_save(&x);
	if(IF_QFULL(&ifp->if_snd)){
		IF_QDROP(&ifp->if_snd);
		local_irq_restore(x);
		retval = -ENOBUFS;
		LOG_INFO("if queue full");
		goto END;
	}
	IF_ENQUEUE(&ifp->if_snd, m);
	local_irq_restore(x);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);

	ifp->if_obytes += len + sizeof(struct ether_header);
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
out:
	LOG_INFO("out.retval:%d", retval);
	return retval;
END:
	if (m)
		m_freem(m);
	goto out;
}















