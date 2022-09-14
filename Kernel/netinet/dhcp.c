/*
 * dhcp.c
 *
 *  Created on: Nov 23, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <kernel/sys/netinet/dhcp.h>
#include <kernel/sys/socketval.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/sys/socket.h>
#include <kernel/net/if.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/sys/sockio.h>
#include <kernel/net/if_ether.h>
#include <kernel/e1000.h>

int setup_ifaddr(struct socket *so, struct in_addr myaddr, struct in_addr netmask, int cmd)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	struct in_aliasreq ifra;
	int ret = 0;
	struct sockaddr_in *sin;
	LOG_INFO("in");
	ifr= kzalloc(sizeof(struct ifreq));
	if (ifr == NULL)
		LOG_ERROR("no memory");

	ifc.ifc_req = ifr;
	ifc.ifc_len = sizeof(*ifr) + sizeof (int);

	ret = ifioctl(so, SIOCGIFCONF, (caddr_t)&ifc);
	if (ret < 0) {
		LOG_ERROR("ifioctl error");
		goto end;
	}

	LOG_INFO("ifterface:%s addr:%d", ifr->ifr_name, ifr->ifr_ifru.ifru_addr.sa_data);

	memset((void*)&ifra, 0, sizeof(ifra));

	memcpy(&ifra.ifra_name, ifr->ifr_name, sizeof(ifr->ifr_name));
	/*
	 * by default, when system is in startup phase, you must
	 * add a multicast route if you want to send dhcp discover
	 * packet out.
	 */
	ifra.ifra_addr.sin_addr.s_addr = myaddr.s_addr;
	ifra.ifra_addr.sin_family = AF_INET;
	ifra.ifra_addr.sin_len = sizeof(ifra.ifra_addr);
	ifra.ifra_broadaddr.sin_addr.s_addr = netmask.s_addr;
	ifra.ifra_broadaddr.sin_family = AF_INET;
	ifra.ifra_broadaddr.sin_len = sizeof(struct sockaddr);
	if (cmd == SIOCSIFADDR) {
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		sin->sin_addr = myaddr;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(struct sockaddr);
		ret = ifioctl(so, cmd, (caddr_t)ifr);
	} else if (cmd == SIOCAIFADDR) {
		ret = ifioctl(so, cmd, (caddr_t)&ifra);
	}

	if (ret < 0) {
		LOG_ERROR("add interface addr error");
	}
end:
	kfree(ifr);
	LOG_INFO("end, ret:%d", ret);
	return ret;
}

void dhcp_build_header(caddr_t buf)
{
	struct dhcp *dp = (struct dhcp *)buf;
	char *cp;
	dp->dp_op = 0x01;
	dp->dp_htype = 0x01;
	dp->dp_hlen = 0x06;
	dp->dp_flags = 0x80;	//tell dhcp server to broadcast dhcp offer packet
	dp->dp_xid = 0x3903f326;	//random

	memcpy(dp->dp_chaddr, adp->mac_addr, ETHER_ADDR_LEN);
	cp = (char *)dp->dp_options;
	*(u32*)cp = 0x63538263;	//dhcp magic
}

struct mbuf *
dhcp_build_mbuf(caddr_t buf, int len)
{
	caddr_t cp;
	struct mbuf *mp, **mp0;
	struct mbuf *top = NULL;
	int offset;

	top = m_gethdr(M_NOWAIT, MT_DATA);
	if (top == NULL) {
		LOG_ERROR("get mbuf data error");
		return NULL;
	}

	cp = buf;
	//offset =  MHLEN - max_protohdr;
	memcpy((void *)(mtod(top, caddr_t)), cp, MHLEN);
	cp += MHLEN;
	top->m_len = MHLEN;
	//top->m_data += max_protohdr;
	top->m_pkthdr.len = MHLEN;
	len -= MHLEN;
	mp0 = &top->m_next;
	while (len > 0) {
		mp = m_get(M_NOWAIT, MT_DATA);
		if (mp == NULL) {
			LOG_ERROR("get mbuf failed");
			goto error;
		}
		*mp0 = mp;
		offset = min(MLEN, len);
		memcpy((void *)(mtod(mp, caddr_t)), cp, offset);
		top->m_pkthdr.len += offset;
		cp += offset;
		mp->m_len = offset;
		len -= offset;
		mp0 = &mp->m_next;
	}
end:
	return top;
error:
	m_freem(top);
	top = NULL;
	goto end;
}

int dhcp_build_discover(struct mbuf **atop)
{
	struct mbuf *top;
	struct dhcp *dp;
	char *cp;
	LOG_INFO("in");

	dp = kzalloc(DHCP_PAYLOAD_MIN);
	if (dp == NULL) {
		LOG_ERROR("no buf");
		return -ENOMEM;
	}

	dhcp_build_header((caddr_t)dp);
	cp = (char *)dp->dp_options;
	cp += 4;
	*cp++ = 53; //stands for type of the DHCP message,
	*cp++ = 1;	//length
	*cp = DHCPDISCOVER;	//discover
	((char *)dp)[DHCP_PAYLOAD_MIN-1] = 0xFF;	//end.

	top = dhcp_build_mbuf((caddr_t)dp, DHCP_PAYLOAD_MIN);
	if (top == NULL) {
		LOG_ERROR("dhcp build mbuf failed");
		kfree(dp);
		return -ENOMEM;
	}
	kfree(dp);
	*atop = top;
	LOG_INFO("end");
	return 0;
}


int dhcp_build_request(struct mbuf **atop, struct in_addr req_ip, struct in_addr srvaddr)
{
	struct mbuf *top;
	struct dhcp *dp;
	char *cp ;

	LOG_INFO("in");

	dp = kzalloc(DHCP_PAYLOAD_MIN);
	if (dp == NULL) {
		LOG_ERROR("no buf");
		return -ENOMEM;
	}

	dhcp_build_header((caddr_t)dp);

	cp = (char *)dp->dp_options;
	cp += 4;
	*cp++ = 53; //stands for type of the DHCP message,
	*cp++ = 1;	//length
	*cp++ = DHCPREQUEST;
	*cp++ = 50;	//requet ip
	*cp++ = 4;	//len
	*(u32 *)cp = req_ip.s_addr;
	cp += 4;
	*cp++ = 54;	//srv addr
	*cp++ = 4;	//len
	*(u32 *)cp = srvaddr.s_addr;
	((char *)dp)[DHCP_PAYLOAD_MIN-1] = 0xFF;	//end.

	top = dhcp_build_mbuf((caddr_t)dp, DHCP_PAYLOAD_MIN);
	if (top == NULL)
		return -ENOMEM;
	kfree(dp);
	*atop = top;
	LOG_INFO("end");
	return 0;
}

int dhcp_get_ip()
{
	struct mbuf *maddr, *top;
	struct socket *so;
	struct sockaddr_in *saddr;
	int ret, offset;
	struct uio auio;
	u8 *cp;
	struct dhcp *dp = NULL;
	struct in_addr myaddr;
	struct in_addr gwaddr;
	struct in_addr netmask;
	struct in_addr dnsaddr;
	struct in_addr srvaddr;

	LOG_INFO("in");
	ret = socreate(AF_INET, &so, SOCK_DGRAM, 0);
	if (ret < 0) {
		LOG_ERROR("create socket failed");
		return ret;
	}
	myaddr.s_addr = 0x8c5ca8c0;
	netmask.s_addr = 0xFFFFFF;
	setup_ifaddr(so, myaddr, netmask, SIOCSIFADDR);
	return 0;
	maddr = m_get(M_NOWAIT, MT_DATA);
	if (maddr == NULL)
		LOG_ERROR("get mbuf error");

	saddr = (mtod(maddr, struct sockaddr_in *));
	saddr->sin_family = AF_INET;
	saddr->sin_addr.s_addr = INADDR_ANY;	//src addr is 0
	saddr->sin_port = htons(68);	//src port is 68
	maddr->m_len = sizeof(struct sockaddr_in);
	//bind local port 68
	sobind(so, maddr);
	//we broadcast discover packet.
	saddr->sin_addr.s_addr = 0xFFFFFFFF;
	saddr->sin_port = htons(67);

	ret = dhcp_build_discover(&top);
	if (ret < 0) {
		LOG_ERROR("build dhcp disover buffer error");
		goto end;
	}
	//eth driver will free top mbuf
	ret = sosend(so, maddr, NULL, top, NULL, 0);
	if (ret < 0) {
		LOG_ERROR("sosend error:%d", (-ret));
		goto end;
	}
	m_freem(maddr);
	maddr = NULL;
	top = NULL;
	auio.uio_resid = 512;
	ret = soreceive(so, &maddr, &auio, &top, NULL, NULL);
	if (ret < 0) {
		LOG_INFO("ret:%d", -ret);
		goto end;
	}
	if (top == NULL) {
		LOG_ERROR("receive error, top == NULL");
		while (12);
	}

	dp = kzalloc(DHCP_PAYLOAD_MIN);

	m_copydata(top, 0, DHCP_PAYLOAD_MIN, (caddr_t)dp);
	if (dp->dp_op != DHCPOFFER) {
		LOG_ERROR("not response");
		goto end;
	}
	m_freem(top);
	top = NULL;
	myaddr = dp->dp_yiaddr;
	LOG_INFO("my addr:%x", myaddr.s_addr);
	cp = dp->dp_options;
	cp += 4; //skip magic cookie

	for (offset=0; *cp != 255; cp += offset) {
		if (*cp == 53) {
			offset = *++cp;
			cp++;
			continue;
		}
		if (*cp == 54) {
			offset = *++cp;
			cp++;
			srvaddr = *(struct in_addr *)cp;
			LOG_INFO("srv addr:%x", srvaddr.s_addr);
			continue;
		}
		if (*cp == 1) {
			offset = *++cp;
			cp++;
			netmask = *(struct in_addr *)cp;
			LOG_INFO("netmask:%x", netmask.s_addr);
			continue;
		}
		if (*cp == 3) {
			offset = *++cp;
			cp++;
			gwaddr = *(struct in_addr *)cp;
			LOG_INFO("router:%x", gwaddr.s_addr);
			continue;
		}
		if (*cp == 6) {
			offset = *++cp;
			cp++;
			dnsaddr = *(struct in_addr *)cp;
			LOG_INFO("dnsaddr:%x", dnsaddr.s_addr);
			continue;
		}
		offset = *++cp;
		cp++;
	}

	ret = dhcp_build_request(&top, myaddr, srvaddr);
	if (ret < 0) {
		LOG_ERROR("build dhcp request packet error");
		goto end;
	}
	saddr = (mtod(maddr, struct sockaddr_in *));
	saddr->sin_family = AF_INET;
	saddr->sin_addr.s_addr = 0xFFFFFFFF;
	saddr->sin_port = htons(67);
	saddr->sin_len = sizeof(struct sockaddr);
	ret = sosend(so, maddr, NULL, top, NULL, 0);
	if (ret < 0 ) {
		panic("so send dhcp request error");
		goto end;
	}

	m_freem(maddr);
	maddr = NULL;
	top = NULL;
	auio.uio_resid = 512;
	ret = soreceive(so, &maddr, &auio, &top, NULL, NULL);
	if (ret < 0) {
		LOG_INFO("ret:%d", -ret);
		return ret;
	}

	//do last check
	memset((void *)dp, 0, DHCP_PAYLOAD_MIN);

	m_copydata(top, 0, DHCP_PAYLOAD_MIN, (caddr_t)dp);
	if (dp->dp_op != 2) {
		LOG_ERROR("not dhcp ack");
		goto end;
	}

	cp = (u8 *)dp->dp_options;
	cp +=6;
	if (*cp != 5) {
		LOG_ERROR("not dhcp ack");
		goto end;
	}

	LOG_INFO("set if addr");
	local_irq_save(&offset);
	ret = setup_ifaddr(so, myaddr, netmask, SIOCSIFADDR);
	local_irq_restore(offset);
	end:
	if (top)
		m_freem(top);
	if (maddr)
		m_freem(maddr);
	if (dp)
		kfree(dp);
	//soclose(so);
	LOG_INFO("end");
	return ret;
}
