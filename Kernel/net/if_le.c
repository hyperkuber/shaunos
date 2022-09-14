/*
 * if_le.c
 *
 *  Created on: Jun 17, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/net/if_ether.h>
#include <kernel/pci.h>
#include <kernel/e1000.h>
#include <kernel/net/if.h>
#include <kernel/net/if_types.h>
#include <kernel/net/if_dl.h>
#include <kernel/net/if_ethersubr.h>
#include <kernel/sys/socket.h>
#include <kernel/mbuf.h>
#include <string.h>
#include <kernel/sys/endian.h>
#include <kernel/malloc.h>


#define NLE	1
struct _le_softc le_softc[NLE];


u8 etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


int ledone(struct ifnet *ifp)
{
	return 0;
}
int leioctl(struct ifnet *ifp, int cmd, caddr_t addr)
{
	return 0;
}

int le_watchdog(int val)
{
	return 0;
}


int leinit()
{

	//call card driver to initialize the softc,
	//after this, we should get the mac address
	LOG_INFO("in");
	if(e1000_init(&le_softc[0]) < 0) {
		LOG_INFO("net card init failed");
		while (1);
	}
	struct ifnet *ifp = &le_softc[0].sc_ac.ac_if;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	ifp->if_name = "eth";
	ifp->if_mtu = ETH_MTU;
	ifp->if_type = IFT_ETHER;
	ifp->if_flags |= (IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_SIMPLEX |IFF_MULTICAST);
	ifp->if_unit = 0;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 14;
	//set call back function
	ifp->if_init = leinit;
	ifp->if_done = ledone;
	ifp->if_ioctl = leioctl;
	ifp->if_output = ether_output;
	//ifp->if_reset = lereset;
	//ifp->if_start = lestart;
	//common interface attach function,
	//register this interface in system
	if_attach(ifp);

	//copy mac address to link layer space
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next){
		if ((sdl = (struct sockaddr_dl *) ifa->ifa_addr) &&
				sdl->sdl_family == AF_LINK &&
				sdl->sdl_alen == ifp->if_addrlen &&
				sdl->sdl_type == IFT_ETHER){
			memcpy((void *)LLADDR(sdl),
							((struct arpcom *)ifp)->ac_enaddr, ifp->if_addrlen);
			break;
		}

	}
	ifp->if_snd.ifq_maxlen = ifq_maxlength;
	LOG_INFO("end");
	return 0;
}

/*
 * this function was called by
 * ether card interrupt routine;
 *
 */
int leread(int unit, char *buf, int len)
{
	struct _le_softc *le = &le_softc[unit];
	struct ether_header *et;
	struct mbuf *m;
	int off, flags;

	le->sc_if.if_ipackets++;
	et = (struct ether_header *)buf;
	et->eth_type = ntohs((u16)et->eth_type);

	len = len - sizeof(struct ether_header) - 4;
	off = 0;
	if (len <= 0){
		LOG_INFO("packet error");
		le->sc_runt++;
		le->sc_if.if_ierrors++;
		return 0;
	}

	flags = 0;
	if (memcmp((void *)et->dest_mac, (void *)le->sc_ac.ac_enaddr, ETHER_ADDR_LEN)) {

		//check broad cast address
		if (memcmp((void *)et->dest_mac,
				(void *)etherbroadcastaddr, sizeof(etherbroadcastaddr)) == 0){
			//it is a broad cast address
			flags |= M_BCAST;
		} else {
			LOG_INFO("not our packet?");
			return 0;
		}
	}


	//check muiltycast address, first byte lowest bit
	if (et->dest_mac[0] & 0x01){
		flags |= M_MCAST;
	}

	//check if we have a filter exist,
	if (le->sc_if.if_bpf){
		//do nothing now
		return 0;
	}

	if (et->eth_type != ETHERTYPE_IP && et->eth_type != ETHERTYPE_ARP) {
		LOG_INFO("for now, we do not support other protocol");
		return 0;
	}

	//drop the ether header
	m = m_devget((char *)(et + 1), len, off, &le->sc_if, NULL);
	if (m == NULL){
		LOG_ERROR("no memory");
		return -ENOMEM;
	}

	m->m_flags |= flags;
	ether_input(&le->sc_if, et, m);
	return 0;
}
