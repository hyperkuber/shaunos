/*
 * udp_usrreq.c
 *
 *  Created on: Aug 26, 2013
 *      Author: Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/mbuf.h>
#include <string.h>
#include <kernel/sys/netinet/udp.h>
#include <kernel/sys/netinet/udp_var.h>
#include <kernel/sys/netinet/ip_icmp.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/protosw.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/sys/socket.h>
#include <kernel/malloc.h>
/*
 * always enable udp checksum
 */
u32 udpcksum = 1;
struct inpcb *udp_last_inpcb;
struct sockaddr_in udp_in;
struct inpcb udb;
struct udpstat udpstat;

u32	udp_sendspace = 9216;		/* really max datagram size */
u32	udp_recvspace = 40 * (1024 + sizeof(struct sockaddr_in));
					/* 40 1K datagrams */

extern int in_cksum(struct mbuf *m, int len);

void udp_init()
{
	udb.inp_next = udb.inp_prev = &udb;
}


int udp_output(struct inpcb *inp, struct mbuf *m,
		struct mbuf *addr, struct mbuf *control)
{
	struct udpiphdr *ui;
	int len = m->m_pkthdr.len;
	struct in_addr laddr;
	int  error = 0;

	LOG_INFO("in");
	if (control)
		m_freem(control);

	if (addr){
		laddr = inp->inp_laddr;
		if (inp->inp_faddr.s_addr != INADDR_ANY){
			error = -EISCONN;
			goto release;
		}


		//TODO:lock
		//TODO:2,this will cost 1/3 time in a udp send procedure.
		error = in_pcbconnect(inp, addr);
		if (error){
			goto release;
		}

	} else {
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
			error = -ENOTCONN;
			goto release;
		}
	}


	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
		error = -ENOBUFS;
		goto release;
	}

	ui = mtod(m, struct udpiphdr *);
	ui->ui_next = ui->ui_prev = 0;
	ui->ui_x1 = 0;
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u16)len + sizeof(struct udphdr));
	ui->ui_src = inp->inp_laddr;
	ui->ui_dst = inp->inp_faddr;
	ui->ui_sport = inp->inp_lport;
	ui->ui_dport = inp->inp_fport;
	ui->ui_ulen = ui->ui_len;

	ui->ui_sum = 0;
	if (udpcksum) {
		if ((ui->ui_sum = in_cksum(m, sizeof(struct udpiphdr) + len)) == 0)
			ui->ui_sum = 0xFFFF;
	}


	((struct ip *)ui)->ip_len = sizeof(struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;
	udpstat.udps_opackets++;
	error = ip_output(m, inp->inp_options, &inp->inp_route,
			inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST), inp->inp_moptions);
	if (addr) {
		in_pcbdisconnect(inp);
		inp->inp_laddr = laddr;
	}

	LOG_INFO("end, error=%d", -error);
	return error;
release:
	m_freem(m);
	LOG_INFO("end, error=%d", -error);
	return error;

}


struct mbuf *
udp_saveopt(caddr_t p, int size, int type)
{
	struct cmsghdr *cp;
	struct mbuf *m;
	if ((m = m_get(M_DONTWAIT, MT_CONTROL)) == NULL)
		return NULL;
	cp = (struct cmsghdr *)mtod(m, struct cmsghdr *);
	bcopy((void *)p, CMSG_DATA(cp), size);
	size += sizeof(*cp);
	m->m_len = size;
	cp->cmsg_len = size;
	cp->cmsg_level = IPPROTO_IP;
	cp->cmsg_type = type;
	return m;
}


void udp_input(struct mbuf *m, int iphlen)
{
	struct ip *ip;
	struct udphdr *uh;
	struct inpcb *inp;
	struct mbuf *opts = NULL;
	int len;
	struct ip save_ip;

	udpstat.udps_ipackets++;

	if (iphlen > sizeof(struct ip)){
		ip_stripoptions(m, NULL);
		iphlen = sizeof(struct ip);
	}

	ip = mtod(m, struct ip *);
	if (m->m_len < iphlen + sizeof(struct udphdr)){
		if ((m = m_pullup(m, iphlen+sizeof(struct udphdr))) == NULL){
			udpstat.udps_badlen++;
			goto bad;
		}

		ip = mtod(m, struct ip *);
		//m_adj(m, len - ip->ip_len);
	}
	uh = (struct udphdr *)((caddr_t)ip +iphlen);

	len = ntohs((u16)uh->uh_ulen);
	if (ip->ip_len != len){
		if (len > ip->ip_len){
			udpstat.udps_badlen++;
			goto bad;
		}
		m_adj(m, len - ip->ip_len);

	}

	save_ip = *ip;

	if (udpcksum && uh->uh_sum){
		((struct ipovly *)ip)->ih_next = NULL;
		((struct ipovly *)ip)->ih_prev = NULL;
		((struct ipovly *)ip)->ih_x1 = 0;
		((struct ipovly *)ip)->ih_len = uh->uh_ulen;

		if ((uh->uh_sum = in_cksum(m, len + sizeof(struct ip)))) {
			udpstat.udps_badsum++;
			m_freem(m);
			return;
		}

	}

	/*
	 * handle multicast and broadcast packet.
	 */

	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
			in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)){
		struct socket *last;

		udp_in.sin_port = uh->uh_sport;
		udp_in.sin_addr = ip->ip_src;
		udp_in.sin_len = sizeof(udp_in);

		m->m_len -= sizeof(struct udpiphdr);
		m->m_data += sizeof(struct udpiphdr);

		last = NULL;
		for (inp=udb.inp_next; inp != &udb; inp = inp->inp_next){
			if (inp->inp_lport != uh->uh_dport)
				continue;
			if(inp->inp_laddr.s_addr != INADDR_ANY){
				if (inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
					continue;
			}

			if (inp->inp_faddr.s_addr != INADDR_ANY){
				if (inp->inp_faddr.s_addr != ip->ip_src.s_addr ||
						inp->inp_fport != uh->uh_sport)
					continue;
			}

			if (last != NULL){
				struct mbuf *n;
				if ((n = m_copy(m, 0, M_COPYALL)) != NULL){
					if(sbappendaddr(&last->so_rcv, (struct sockaddr *)&udp_in,
							n, NULL	) == 0){
						m_freem(n);
						udpstat.udps_fullsock++;
					} else
						sorwakeup(last);
				}
			}

			last = inp->inp_socket;

			if((last->so_options & (SO_REUSEPORT | SO_REUSEADDR)) == 0 )
				break;
		}

		if (last == NULL){
			udpstat.udps_noportbcast++;
			goto bad;
		}

		if (sbappendaddr(&last->so_rcv, (struct sockaddr *)&udp_in, m, NULL) == 0){
			udpstat.udps_fullsock++;
			goto bad;
		}
		sorwakeup(last);
		return;
	}

	/*
	 * locate pcb for unicast datagram
	 *
	 * one bind cache is useless, little than 3% match
	 * and this should be updated.
	 */
	inp = udp_last_inpcb;
	if (inp->inp_lport != uh->uh_dport ||
			inp->inp_fport != uh->uh_sport ||
			inp->inp_faddr.s_addr != ip->ip_src.s_addr ||
			inp->inp_laddr.s_addr != ip->ip_dst.s_addr) {
		inp = in_pcblookup(&udb, ip->ip_src, uh->uh_sport,
				ip->ip_dst, uh->uh_dport, INPLOOKUP_WILDCARD);

		if (inp)
			udp_last_inpcb = inp;
		udpstat.udpps_pcbcachemiss++;
	}

	if (inp == NULL){
		udpstat.udps_noport++;
		if (m->m_flags & (M_BCAST | M_MCAST)){
			udpstat.udps_noportbcast++;
			goto bad;
		}

		*ip = save_ip;
		ip->ip_len += iphlen;
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
		return;
	}

	udp_in.sin_port = uh->uh_sport;
	udp_in.sin_addr = ip->ip_src;

	if (inp->inp_flags & INP_CONTROLOPTS){
		struct mbuf **mp = &opts;
		if(inp->inp_flags & INP_RECVDSTADDR){
			*mp = udp_saveopt((caddr_t)&ip->ip_dst, sizeof(struct in_addr), INP_RECVDSTADDR);
			if (*mp)
				mp = &(*mp)->m_next;
		}

	}

	iphlen += sizeof(struct udphdr);
	m->m_len -= iphlen;
	m->m_pkthdr.len -= iphlen;
	m->m_data += iphlen;

	if (sbappendaddr(&inp->inp_socket->so_rcv, (struct sockaddr *)&udp_in, m, opts) == 0){
		udpstat.udps_fullsock++;
		goto bad;
	}
	sorwakeup(inp->inp_socket);
	return;
bad:
	m_freem(m);
	if (opts)
		m_freem(opts);
}

void
udp_notify(struct inpcb *inp, int errno)
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
}

void udp_ctlinput(int cmd, struct sockaddr *sa, struct ip *ip)
{
	struct udphdr *uh;
	extern struct in_addr zeroin_addr;
	extern u8	inetctlerrmap[];


	if (!PRC_IS_REDIRECT(cmd) && ((unsigned) cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return ;

	if (ip){
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&udb, sa, uh->uh_dport, ip->ip_src, uh->uh_sport, cmd, udp_notify);
	} else{
		in_pcbnotify(&udb, sa, 0, zeroin_addr, 0, cmd, udp_notify);
	}

}


static void
udp_detach(struct inpcb *inp)
{
	LOG_INFO("in");
	if (inp == udp_last_inpcb)
		udp_last_inpcb = &udb;
	in_pcbdetach(inp);
	LOG_INFO("in");
}

int
udp_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr, struct mbuf *control)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;

	LOG_INFO("in");

	//ioctl syscall
	if (req == PRU_CONTROL){
		LOG_INFO("ifp:%x", (u32)control);
		return in_control(so, (u32)m, (caddr_t)addr, (struct ifnet *)control);
	}

	if (inp == NULL && req != PRU_ATTACH){
		error = -EINVAL;
		goto release;
	}


	switch (req) {
	case PRU_ATTACH:
		LOG_INFO("udp attach");
		if (inp != NULL){
			error = -EINVAL;
			break;
		}

		error = in_pcballoc(so, &udb);
		if (error)
			break;

		error = soreserve(so, udp_sendspace, udp_recvspace);
		if (error)
			break;
		((struct inpcb *) so->so_pcb)->inp_ip.ip_ttl = ip_defttl;
		break;
	case PRU_DETACH:
		udp_detach(inp);
		break;
	case PRU_BIND:
		error = in_pcbbind(inp, addr);
		break;
	case PRU_LISTEN:
		error = -EOPNOTSUPP;
		break;
	case PRU_CONNECT:
		if (inp->inp_faddr.s_addr != INADDR_ANY){
			error = -EISCONN;
			break;
		}
		error = in_pcbconnect(inp, addr);
		if (error == 0)
			soisconnected(so);
		break;
	case PRU_CONNECT2:	//socketpaire
		error = -EOPNOTSUPP;
		break;
	case PRU_ACCEPT:
		error = -EOPNOTSUPP;
		break;
	case PRU_DISCONNECT:
		if (inp->inp_faddr.s_addr == INADDR_ANY){
			error = -ENOTCONN;
			break;
		}

		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		so->so_state &= ~SS_ISCONNECTED;
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;
	case PRU_SEND:
		LOG_INFO("udp send");
		return (udp_output(inp, m, addr, control));
	case PRU_ABORT:
		soisdisconnected(so);
		udp_detach(inp);
		break;
	case PRU_SOCKADDR:
		in_setsockaddr(inp, addr);
		break;
	case PRU_PEERADDR:
		in_setpeeraddr(inp, addr);
		break;
	case PRU_SENSE:
		return 0;
	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error = -EOPNOTSUPP;
		break;
	case PRU_RCVD:
	case PRU_RCVOOB:
		return (-EOPNOTSUPP);
	default:
		LOG_ERROR("udp_userreq");
	}

release:
	if (control){
		LOG_ERROR("udp control data retained");
		m_freem(control);
	}
	if (m)
		m_freem(m);
	return error;

}









