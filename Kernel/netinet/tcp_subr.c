/*
 * tcp_subr.c
 *
 *  Created on: Sep 21, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/netinet/tcp.h>
#include <kernel/sys/netinet/tcp_var.h>
#include <kernel/sys/netinet/tcp_seq.h>
#include <kernel/sys/netinet/tcp_timer.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/mbuf.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/sys/netinet/tcp_fsm.h>


struct inpcb tcb;
struct tcpstat	tcpstat;
struct tcpcb;
u32	tcp_now;
u32	tcp_iss;
extern int in_cksum(struct mbuf *m, int len);
/* patchable/settable parameters for tcp */
int tcp_mssdflt = TCP_MSS;
int tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
int	tcp_do_rfc1323 = 1;

/*
 * Flags used when sending segments in tcp_output.
 * Basic flags (TH_RST,TH_ACK,TH_SYN,TH_FIN) are totally
 * determined by state, with the proviso that TH_FIN is sent only
 * if all data queued for output is included in the segment.
 */
u8	tcp_outflags[TCP_NSTATES] = {
    TH_RST|TH_ACK, 0, TH_SYN, TH_SYN|TH_ACK,
    TH_ACK, TH_ACK,
    TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_ACK, TH_ACK,
};


void
tcp_init()
{
	//TODO:
	tcp_iss	= 1;
	tcb.inp_next = tcb.inp_prev = &tcb;

	if (max_protohdr < sizeof(struct tcpiphdr))
		max_protohdr = sizeof(struct tcpiphdr);

	if (max_linkhdr + sizeof(struct tcpiphdr) > MHLEN)
		LOG_ERROR("max_linkhdr");
}

struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	struct tcpcb *tp;

	tp = kzalloc(sizeof(*tp));
	if (tp == NULL)
		return NULL;

	tp->seg_next = tp->seg_prev = (struct tcpiphdr *)tp;


	tp->t_maxseg = tcp_mssdflt;
	tp->t_flags = tcp_do_rfc1323 ? (TF_REQ_SCALE | TF_REQ_TSTMP) : 0;
	tp->t_inpcb = inp;

	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = tcp_rttdflt * PR_SLOWHZ << 2;
	tp->t_rttmin = TCPTV_MIN;

	TCPT_RANGESET(tp->t_rxtcur,
			((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
			TCPTV_MIN, TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;

	inp->inp_ip.ip_ttl = ip_defttl;
	inp->inp_ppcb = (caddr_t)tp;
	return tp;
}



/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void
tcp_quench(struct inpcb *inp, int errno)
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp)
		tp->snd_cwnd = tp->t_maxseg;
}

struct tcpiphdr *
tcp_template(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct mbuf *m;
	struct tcpiphdr *n;

	if ((n = tp->t_template) == NULL) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return NULL;
		m->m_len = sizeof(struct tcpiphdr *);
		n = mtod(m, struct tcpiphdr *);
		tp->t_tempbuf = m;
	}

	n->ti_next = n->ti_prev = NULL;
	n->ti_x1	= 0;
	n->ti_pr	= IPPROTO_TCP;
	n->ti_len = htons(sizeof(struct tcpiphdr) - sizeof(struct ip));
	n->ti_src = inp->inp_laddr;
	n->ti_dst = inp->inp_faddr;
	n->ti_sport = inp->inp_lport;
	n->ti_dport = inp->inp_fport;
	n->ti_seq = 0;
	n->ti_ack = 0;
	n->ti_x2 = 0;
	n->ti_off = 5;
	n->ti_flags = 0;
	n->ti_win = 0;
	n->ti_sum = 0;
	n->ti_urp = 0;
	return 0;
}

void tcp_respond(struct tcpcb *tp, struct tcpiphdr *ti,
		struct mbuf *m, u32 ack, u32 seq, int flags)
{
	int tlen;
	int win = 0;
	struct route *ro = NULL;

	if (tp) {
		win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
		ro = &tp->t_inpcb->inp_route;

	}

	if (m == NULL) {
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return;
		tlen = 0;
		m->m_data += max_linkhdr;
		*mtod(m, struct tcpiphdr *) = *ti;

		ti = mtod(m, struct tcpiphdr *);
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = NULL;
		m->m_data = (caddr_t)ti;
		m->m_len = sizeof(struct tcpiphdr);
		tlen = 0;
#define xchg(a, b, type)	{	type t; (t)=(a); (a)=(b); (b)=(t);}
		xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u32);
		xchg(ti->ti_dport, ti->ti_sport, u16);
#undef xchg

	}


	ti->ti_len = htons((u16)(sizeof(struct tcphdr) + tlen));
	tlen += sizeof(struct tcpiphdr);
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;
	ti->ti_next = ti->ti_prev = NULL;
	ti->ti_x1 = 0;

	ti->ti_seq = htonl(seq);
	ti->ti_ack = htonl(ack);

	ti->ti_x2	= 0;
	ti->ti_off = sizeof(struct tcphdr) >> 2;
	ti->ti_flags = flags;

	if (tp)
		ti->ti_win = htons((u16)(win >> tp->rcv_scale));
	else
		ti->ti_win =  htons((u16)win);

	ti->ti_urp = 0;
	ti->ti_sum = 0;
	ti->ti_sum = in_cksum(m, tlen);
	((struct ip *)ti)->ip_len = tlen;
	((struct ip *)ti)->ip_ttl = ip_defttl;
	ip_output(m, NULL, ro, 0, NULL);
}


void tcp_drain()
{
	return;
}



struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct tcpiphdr *t;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	struct mbuf *m;
	struct rtentry *rt;


	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
			(rt = inp->inp_route.ro_rt) &&
			((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr != INADDR_ANY) {
		u32 i;
		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt * (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
				rt->rt_rmx.rmx_rtt = (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}

		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar * (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));

			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar = (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}

		if (((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
				(i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh) ||
				i < (rt->rt_rmx.rmx_sendpipe / 2)) {

			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;

			if (i < 2 )
				i = 2;

			i *= (u32)(tp->t_maxseg + sizeof(struct tcpiphdr));
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh = (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}

	t = tp->seg_next;
	while (t != (struct tcpiphdr *)tp) {
		t = (struct tcpiphdr *)t->ti_next;
		m = REASS_MBUF((struct tcpiphdr *)t->ti_prev);
		remque(t->ti_prev);
		m_freem(m);
	}

	if (tp->t_template)
		m_free(tp->t_tempbuf);

	kfree(tp);
	inp->inp_ppcb = NULL;


	soisdisconnected(so);

	if(inp == tcp_last_inpcb)
		tcp_last_inpcb = &tcb;
	in_pcbdetach(inp);
	tcpstat.tcps_closed++;
	return NULL;
}




struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		tcp_output(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;

	if (errno == -ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;

	so->so_error = errno;
	return tcp_close(tp);
}



void tcp_notify(struct inpcb *inp, int error)
{
	struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
	struct socket *so = inp->inp_socket;

	if (tp->t_state == TCPS_ESTABLISHED && (error == -EHOSTUNREACH || error == -ENETUNREACH
			|| error == -EHOSTDOWN))
		return;
	else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 && tp->t_softerror)
		so->so_error = error;
	else
		tp->t_softerror = error;

	//wakeup((caddr_t)&so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
}




void tcp_ctlinput(int cmd, struct sockaddr *sa, struct ip *ip)
{
	struct tcphdr *th;
	extern struct in_addr zeroin_addr;
	extern u8	inetctlerrmap[];
	void (*notify)(struct inpcb *, int) = tcp_notify;

	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if(!PRC_IS_REDIRECT(cmd) && ((unsigned)cmd > PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;

	if (ip) {
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&tcb, sa, th->th_dport, ip->ip_src, th->th_sport, cmd, notify);
	} else {
		in_pcbnotify(&tcb, sa, 0, zeroin_addr, 0, cmd, notify);
	}
}


void
tcp_trace(short act, short ostate, struct tcpcb *tp, struct tcpiphdr *ti, int req)
{
	return;
}




