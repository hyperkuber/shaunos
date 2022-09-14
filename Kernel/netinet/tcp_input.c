/*
 * tcp_input.c
 *
 *  Created on: Sep 25, 2013
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
#include <kernel/sys/netinet/tcp_fsm.h>
#include <string.h>



struct	inpcb *tcp_last_inpcb = &tcb;

#define TCP_PAWS_IDLE	(24 * 24 * 60 * 60 * PR_SLOWHZ)





int	tcprexmtthresh = 3;
struct	tcpiphdr tcp_saveti;
/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)


#define TCP_REASS(tp, ti, m, so, flags) \
	{	\
		if ((ti)->ti_seq == (tp)->rcv_nxt && \
				(tp)->seg_next == (struct tcpiphdr *)(tp) && 	\
				(tp)->t_state == TCPS_ESTABLISHED) {	\
			(tp)->t_flags |= TF_DELACK;	\
			(tp)->rcv_nxt += (ti)->ti_len;	\
			(flags) = (ti)->ti_flags & TH_FIN;	\
			tcpstat.tcps_rcvpack++;	\
			tcpstat.tcps_rcvbyte += (ti)->ti_len;	\
			sbappend(&(so)->so_rcv, (m));	\
			sorwakeup(so);	\
		}	 else {	\
			(flags) = tcp_reass((tp), (ti), (m));	\
			(tp)->t_flags |= TF_ACKNOW;	\
		}	\
	}

extern int in_cksum(struct mbuf *m, int len);

void tcp_xmit_timer(struct tcpcb *tp, short rtt)
{
	short delta;
	tcpstat.tcps_rttupdated++;

	if (tp->t_srtt != 0) {
		delta = rtt - 1 - (tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;

		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
	} else {
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
	}

	tp->t_rtt = 0;
	tp->t_rxtshift = 0;

	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp), tp->t_rttmin, TCPTV_REXMTMAX);

	tp->t_softerror = 0;
}

int tcp_mss(struct tcpcb *tp, int offer)
{
	struct route *ro;
	struct rtentry *rt;
	struct ifnet *ifp;
	int rtt, mss;
	u32	bufsize;
	struct inpcb *inp;
	struct socket *so;
	extern int tcp_mssdflt;


	inp = tp->t_inpcb;
	ro = &inp->inp_route;
	if ((rt = ro->ro_rt) == NULL) {
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(ro->ro_dst);
			((struct sockaddr_in *)&ro->ro_dst)->sin_addr = inp->inp_faddr;
			rtalloc(ro);
		}

		if ((rt = ro->ro_rt) == NULL)
			return tcp_mssdflt;
	}

	ifp = rt->rt_ifp;
	so = inp->inp_socket;

	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		if (rt->rt_rmx.rmx_locks & RTV_RTT)
			tp->t_rttmin = rtt / (RTM_RTTUNIT / PR_SLOWHZ);
		tp->t_srtt = rtt / (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));

		if(rt->rt_rmx.rmx_rttvar)
			tp->t_rttvar = rt->rt_rmx.rmx_rttvar / (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
		else
			tp->t_rttvar = tp->t_srtt * TCP_RTTVAR_SCALE / TCP_RTT_SCALE;

		TCPT_RANGESET(tp->t_rxtcur, ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1, tp->t_rttmin, TCPTV_REXMTMAX);

	}

	if (rt->rt_rmx.rmx_mtu)
		mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpiphdr);
	else {
		mss = ifp->if_mtu - sizeof(struct tcpiphdr);
#if (MCLBYTES & (MCLBYTES -1)) == 0
		if (mss > MCLBYTES)
			mss &= ~(MCLBYTES -1);
#else
		if (mss > MCLBYTES)
			mss = mss / (MCLBYTES * MCLBYTES);
#endif
		if (!in_localaddr(inp->inp_faddr))
			mss = min(mss, tcp_mssdflt);
	}

	if (offer)
		mss = min(mss, offer);
	mss = max(mss, 32);

	if (mss < tp->t_maxseg || offer != 0) {
		if ((bufsize = rt->rt_rmx.rmx_sendpipe) == 0)
			bufsize = so->so_snd.sb_hiwat;
		if (bufsize < mss)
			mss = bufsize;
		else {
			bufsize = roundup(bufsize, mss);
			if (bufsize > sb_max)
				bufsize = sb_max;
			sbreserve(&so->so_snd, bufsize);
		}

		tp->t_maxseg = mss;

		if((bufsize = rt->rt_rmx.rmx_recvpipe) == 0)
			bufsize = so->so_rcv.sb_hiwat;
		if (bufsize > mss) {
			bufsize = roundup(bufsize, mss);
			if (bufsize > sb_max)
				bufsize = sb_max;
			sbreserve(&so->so_rcv, bufsize);
		}
	}

	tp->snd_cwnd = mss;
	if (rt->rt_rmx.rmx_ssthresh) {
		tp->snd_ssthresh = max(2*mss, rt->rt_rmx.rmx_ssthresh);
	}

	return mss;
}


int tcp_reass(struct tcpcb *tp, struct tcpiphdr *ti, struct mbuf *m)
{
	struct tcpiphdr *q;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

	if (ti == 0)
		goto present;

	for (q=tp->seg_next; q != (struct tcpiphdr *)tp;
			q = (struct tcpiphdr *)q->ti_next) {
		if (SEQ_GT(q->ti_seq, ti->ti_seq))
			break;
	}

	if ((struct tcpiphdr *)q->ti_prev != (struct tcpiphdr *)tp) {
		int i;
		q = (struct tcpiphdr *)q->ti_prev;
		i = q->ti_seq + q->ti_len - ti->ti_seq;
		if (i > 0) {
			if (i >= ti->ti_len) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte+= ti->ti_len;
				m_freem(m);
				return 0;
			}

			m_adj(m, i);
			ti->ti_len -= i;
			ti->ti_seq += i;
		}

		q = (struct tcpiphdr *)(q->ti_next);
	}

	tcpstat.tcps_rcvoopack++;
	tcpstat.tcps_rcvoobyte += ti->ti_len;
	REASS_MBUF(ti) = m;

	while (q != (struct tcpiphdr *)tp) {
		int i = (ti->ti_seq + ti->ti_len) - q->ti_seq;
		if (i <= 0)
			break;
		if (i < q->ti_len) {
			q->ti_seq += i;
			q->ti_len -= i;
			m_adj(REASS_MBUF(q), i);
			break;
		}

		q = (struct tcpiphdr *)q->ti_next;
		m = REASS_MBUF((struct tcpiphdr *)q->ti_prev);
		remque(q->ti_prev);

		m_freem(m);
	}

	insque(ti, q->ti_prev);
present:
	if (TCPS_HAVERCVDSYN(tp->t_state) == 0)
		return 0;

	ti = tp->seg_next;
	if (ti == (struct tcpiphdr *)tp || ti->ti_seq != tp->rcv_nxt)
		return 0;

	if (tp->t_state == TCPS_SYN_RECEIVED && ti->ti_len)
		return 0;

	do {
		tp->rcv_nxt += ti->ti_len;
		flags = ti->ti_flags & TH_FIN;
		remque(ti);

		m = REASS_MBUF(ti);

		ti = (struct tcpiphdr *)ti->ti_next;
		if (so->so_state & SS_CANTRCVMORE)
			m_freem(m);
		else
			sbappend(&so->so_rcv, m);
	} while (ti != (struct tcpiphdr *)tp && ti->ti_seq == tp->rcv_nxt);
	sorwakeup(so);
	return flags;
}


void tcp_dooptions(struct tcpcb *tp, u8 *cp, int cnt,
		struct tcpiphdr *ti, int *ts_present, u32 *ts_val, u32 *ts_ecr)
{
	u16 mss;
	int opt, optlen;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}

		switch (opt) {
		default:
			continue;
		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(ti->ti_flags & TH_SYN))
				continue;
			//bcopy((char *)cp + 2, (char *)&mss, sizeof(mss));
			mss = *(u16 *)(cp+2);
			NTOHS(mss);
			tcp_mss(tp, mss);
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(ti->ti_flags & TH_SYN))
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = min(cp[2], TCP_MAX_WINSHIFT);
			break;
		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			*ts_present = 1;
			*ts_val = *(u32 *)(cp+2);
			*ts_ecr = *(u32 *)(cp+6);
			NTOHL(*ts_val);
			NTOHL(*ts_ecr);
			break;
		}
	}
}

void tcp_pulloutofband(struct socket *so, struct tcpiphdr *ti, struct mbuf *m)
{
	int cnt = ti->ti_urp -1;
	char *cp;
	struct tcpcb *tp;
	while (cnt >= 0) {
		if (m->m_len > cnt) {
			cp = mtod(m, caddr_t) + cnt;
			tp = (struct tcpcb *)(((struct inpcb *)(so->so_pcb))->inp_ppcb);
			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (m->m_len - cnt -1));
			m->m_len--;
			return;
		}

		cnt -= m->m_len;
		m = m->m_next;
		if (m == NULL)
			break;
	}
}


void tcp_input(struct mbuf *m, int iphlen)
{
	struct tcpiphdr *ti;
	struct inpcb *inp;
	caddr_t optp = NULL;
	int optlen;
	int len, tlen, off;
	struct tcpcb *tp = NULL;
	int tiflags;
	struct socket *so;
	int todrop, acked, ourfinisacked, needoutput = 0;
	s16	ostate;
	struct in_addr laddr;
	int dropsocket = 0;
	int iss = 0;
	u32	tiwin, ts_val, ts_ecr;
	int ts_present = 0;
	int win;
	tcpstat.tcps_rcvtotal++;


	ti = mtod(m, struct tcpiphdr *);
	if(iphlen > sizeof(struct ip))
		ip_stripoptions(m, NULL);

	if (m->m_len < sizeof(struct tcpiphdr)) {
		if ((m = m_pullup(m, sizeof(struct tcpiphdr))) == NULL){
			tcpstat.tcps_rcvshort++;
			return;
		}
		ti = mtod(m, struct tcpiphdr *);
	}

	tlen = ((struct ip *)ti)->ip_len;
	len = sizeof(struct ip) + tlen;

	ti->ti_next = ti->ti_prev = 0;
	ti->ti_x1 = 0;
	ti->ti_len = (u16) tlen;
	HTONS(ti->ti_len);

	if ((ti->ti_sum = in_cksum(m, len))) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}


	off = ti->ti_off << 2;
	if (off < sizeof(struct tcphdr) || off > tlen){
		tcpstat.tcps_rcvbadoff++;
		goto drop;
	}

	tlen -= off;
	ti->ti_len = tlen;

	if (off > sizeof(struct tcphdr)) {
		if (m->m_len < sizeof(struct ip) + off) {
			if ((m = m_pullup(m, sizeof(struct ip) + off)) == NULL) {
				tcpstat.tcps_rcvshort++;
				return;
			}
			ti = mtod(m, struct tcpiphdr *);
		}
		optlen = off - sizeof(struct tcphdr);
		optp = mtod(m, caddr_t) + sizeof(struct tcpiphdr);

		if ((optlen == TCPOLEN_TSTAMP_APPA ||
				(optlen > TCPOLEN_TSTAMP_APPA &&
						optp[TCPOLEN_TSTAMP_APPA] == TCPOPT_EOL	)) &&
						*(u32 *)optp == htonl(TCPOPT_TSTAMP_HDR) &&
						(ti->ti_flags & TH_SYN) == 0) {
			ts_present = 1;
			ts_val = ntohl(*(u32 *)(optp + 4));
			ts_ecr = ntohl(*(u32 *)(optp + 8));
			optp = NULL;
		}

	}


	tiflags = ti->ti_flags;
	NTOHL(ti->ti_seq);
	NTOHL(ti->ti_ack);
	NTOHS(ti->ti_win);
	NTOHS(ti->ti_urp);

	findpcb:
	inp = tcp_last_inpcb;
	if (inp->inp_lport != ti->ti_dport || inp->inp_fport != ti->ti_sport ||
			inp->inp_faddr.s_addr != ti->ti_src.s_addr ||
			inp->inp_laddr.s_addr != ti->ti_dst.s_addr ) {
		inp = in_pcblookup(&tcb, ti->ti_src, ti->ti_sport, ti->ti_dst, ti->ti_dport, INPLOOKUP_WILDCARD);
		if (inp)
			tcp_last_inpcb = inp;
		++tcpstat.tcps_pcbcachemiss;
	}

	if (inp == NULL)
		goto dropwithreset;
	tp = intotcpcb(inp);
	if (tp == NULL)
		goto dropwithreset;

	if (tp->t_state == TCPS_CLOSED)
		goto drop;

	if ((tiflags & TH_SYN) == 0)
		tiwin = ti->ti_win << tp->snd_scale;
	else
		tiwin = ti->ti_win;

	so = inp->inp_socket;
	if (so->so_options & (SO_DEBUG | SO_ACCEPTCONN)) {
		if (so->so_options & SO_DEBUG) {
			ostate = tp->t_state;
			tcp_saveti = *ti;
		}

		if (so->so_options & SO_ACCEPTCONN) {
			so = sonewconn(so, 0);
			if (so == NULL)
				goto drop;

			dropsocket++;
			inp = (struct inpcb *)so->so_pcb;
			inp->inp_laddr = ti->ti_dst;
			inp->inp_lport = ti->ti_dport;
			inp->inp_options = ip_srcroute();
			tp = intotcpcb(inp);
			tp->t_state = TCPS_LISTEN;

			while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
					TCP_MAXWIN << tp->request_r_scale < so->so_rcv.sb_hiwat)
				tp->request_r_scale++;
		}
	}

	tp->t_idle = 0;
	tp->t_timer[TCPT_KEEP] = tcp_keepidle;

	if (optp && tp->t_state != TCPS_LISTEN)
		tcp_dooptions(tp, (u8 *)optp, optlen, ti, &ts_present, &ts_val, &ts_ecr);

	if (tp->t_state == TCPS_ESTABLISHED &&
			(tiflags & (TH_SYN | TH_FIN | TH_RST | TH_URG| TH_ACK)) == TH_ACK &&
					(!ts_present || TSTMP_GEQ(ts_val, tp->ts_recent)) &&
					ti->ti_seq == tp->rcv_nxt &&
					tiwin && tiwin == tp->snd_nxt &&
					tp->snd_nxt == tp->snd_max) {
		if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
				SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = ts_val;
		}

		if (ti->ti_len == 0) {
			if (SEQ_GT(ti->ti_ack, tp->snd_una) &&
					SEQ_LEQ(ti->ti_ack, tp->snd_max) &&
					tp->snd_cwnd >= tp->snd_wnd) {
				++tcpstat.tcps_predack;
				if (ts_present)
					tcp_xmit_timer(tp, tcp_now - ts_ecr +1);
				else if (tp->t_rtt && SEQ_GT(ti->ti_ack, tp->t_rtseq))
					tcp_xmit_timer(tp, tp->t_rtt);

				acked = ti->ti_ack - tp->snd_una;
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;
				sbdrop(&so->so_snd, acked);
				tp->snd_una = ti->ti_ack;
				m_freem(m);


				if (tp->snd_una == tp->snd_max)
					tp->t_timer[TCPT_REXMT] = 0;
				else if (tp->t_timer[TCPT_PERSIST] == 0)
					tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

				if (so->so_snd.sb_flags & SB_NOTIFY)
					sowwakeup(so);
				if (so->so_snd.sb_cc)
					tcp_output(tp);
				return;
			}
		} else if (ti->ti_ack == tp->snd_una &&
				tp->seg_next == (struct tcpiphdr *)tp &&
				ti->ti_len <= sbspace(&so->so_rcv)) {
			++tcpstat.tcps_preddat;
			tp->rcv_nxt += ti->ti_len;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += ti->ti_len;

			m->m_data += sizeof(struct tcpiphdr) + off - sizeof(struct tcphdr);
			m->m_len -= sizeof(struct tcpiphdr ) + off - sizeof(struct tcphdr);
			sbappend(&so->so_rcv, m);
			sorwakeup(so);
			tp->t_flags |= TF_DELACK;
			return;
		}
	}


	m->m_data += sizeof(struct tcpiphdr) + off - sizeof(struct tcphdr);
	m->m_len -= sizeof(struct tcpiphdr) + off - sizeof(struct tcphdr);


	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = max(win, (int)(tp->rcv_adv - tp->rcv_nxt));


	switch (tp->t_state) {
	case TCPS_LISTEN:
	{
		struct mbuf *am;
		struct sockaddr_in *sin;
		if (tiflags & TH_RST)
			goto drop;
		if (tiflags & TH_ACK)
			goto dropwithreset;
		if ((tiflags & TH_SYN) == 0)
			goto drop;

		if ((m->m_flags & (M_BCAST | M_MCAST)) || IN_MULTICAST(ntohl(ti->ti_dst.s_addr)))
			goto drop;

		am = m_get(M_DONTWAIT, MT_SONAME);
		if (am == NULL)
			goto drop;

		am->m_len = sizeof(struct sockaddr_in);

		sin = mtod(am, struct sockaddr_in *);
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ti->ti_src;
		sin->sin_port = ti->ti_sport;
		//bzero(sin->sin_zero, sizeof(sin->sin_zero));
		laddr = inp->inp_laddr;
		if (inp->inp_laddr.s_addr == INADDR_ANY)
			inp->inp_laddr = ti->ti_dst;
		if (in_pcbconnect(inp, am)) {
			inp->inp_laddr = laddr;
			m_free(am);
			goto drop;
		}
		m_free(am);
		tp->t_template = tcp_template(tp);
		if (tp->t_template == NULL) {
			tp = tcp_drop(tp, -ENOBUFS);
			dropsocket = 0;
			goto drop;
		}
		if (optp) {
			tcp_dooptions(tp, (u8 *)optp, optlen, ti, &ts_present, &ts_val, &ts_ecr);
		}

		if (iss)
			tp->iss = iss;
		else
			tp->iss = tcp_iss;


		tcp_iss += TCP_ISSINCR / 2;
		tp->irs = ti->ti_seq;
		tcp_sendseqinit(tp);
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;

		dropsocket = 0;
		tcpstat.tcps_accepts++;
		goto trimthenstep6;
	}
	case TCPS_SYN_SENT:
		if ((tiflags & TH_ACK) && (SEQ_LEQ(ti->ti_ack, tp->iss) || SEQ_GT(ti->ti_ack, tp->snd_max)))
			goto dropwithreset;
		if (tiflags & TH_RST) {
			if (tiflags & TH_ACK)
				tp = tcp_drop(tp, -ECONNREFUSED);
			goto drop;
		}

		if ((tiflags & TH_SYN) == 0)
			goto drop;

		if (tiflags & TH_ACK) {
			tp->snd_una = ti->ti_ack;
			if (SEQ_LT(tp->snd_nxt, tp->snd_una))
				tp->snd_nxt = tp->snd_una;
		}

		tp->t_timer[TCPT_REXMT] = 0;
		tp->irs = ti->ti_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		if ((tiflags & TH_ACK) && SEQ_GT(tp->snd_una, tp->iss)) {
			tcpstat.tcps_connects++;
			soisconnected(so);
			tp->t_state = TCPS_ESTABLISHED;
			if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) == (TF_RCVD_SCALE | TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}

			tcp_reass(tp, NULL, NULL);

			if (tp->t_rtt)
				tcp_xmit_timer(tp, tp->t_rtt);
		} else
			tp->t_state = TCPS_SYN_RECEIVED;
		trimthenstep6:
		ti->ti_seq++;
		if (ti->ti_len > tp->rcv_wnd) {
			todrop = ti->ti_len - tp->rcv_wnd;
			m_adj(m, -todrop);
			ti->ti_len = tp->rcv_wnd;
			tiflags &= ~TH_FIN;
			tcpstat.tcps_rcvpackafterwin++;
			tcpstat.tcps_rcvbyteafterwin += todrop;
		}

		tp->snd_wl1 = ti->ti_seq -1;
		tp->rcv_up = ti->ti_seq;
		goto step6;
	}

	if (ts_present && (tiflags & TH_RST) == 0 &&
			tp->ts_recent && TSTMP_LT(ts_val, tp->ts_recent)) {
		if ((int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
			tp->ts_recent = 0;

		} else {
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += ti->ti_len;
			tcpstat.tcps_pawsdrop++;
			goto dropafterack;
		}

	}

	todrop = tp->rcv_nxt - ti->ti_seq;
	if (todrop > 0) {
		if (tiflags & TH_SYN) {
			tiflags &= ~ TH_SYN;
			ti->ti_seq++;

			if (ti->ti_urp > 1)
				ti->ti_urp--;
			else
				tiflags &= ~TH_URG;

			todrop--;
		}

		if (todrop > ti->ti_len || ((todrop == ti->ti_len) && (tiflags & TH_FIN) == 0) ) {
			tiflags &= ~TH_FIN;

			tp->t_flags |= TF_ACKNOW;
			todrop = ti->ti_len;
			tcpstat.tcps_rcvdupbyte += todrop;
			tcpstat.tcps_rcvduppack++;
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
		}

/*		if (todrop >= ti->ti_len) {
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += ti->ti_len;

			if ((tiflags & TH_FIN) && (todrop == ti->ti_len +1)) {
				todrop = ti->ti_len;
				tiflags &= ~TH_FIN;
				tp->t_flags |= TF_ACKNOW;
			} else {
				if (todrop != 0 || (tiflags & TH_ACK) == 0) {
					goto dropafterack;
				}
			}
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
		}
*/
		m_adj(m, todrop);
		ti->ti_seq += todrop;
		ti->ti_len -= todrop;
		if (ti->ti_urp > todrop)
			ti->ti_urp -= todrop;
		else {
			tiflags &= ~ TH_URG;
			ti->ti_urp = 0;
		}
	}


	if ((so->so_state & SS_NOFDREF) &&
			tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) {
		tp = tcp_close(tp);
		tcpstat.tcps_rcvafterclose++;
		goto dropwithreset;
	}

	todrop = (ti->ti_seq + ti->ti_len) - (tp->rcv_nxt - tp->rcv_wnd);
	if (todrop > 0) {
		tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= ti->ti_len ) {
			tcpstat.tcps_rcvpackafterwin += ti->ti_len;
			if ((tiflags & TH_SYN) && tp->t_state == TCPS_TIME_WAIT && SEQ_GT(ti->ti_seq, tp->rcv_nxt)) {
				iss = tp->rcv_nxt + TCP_ISSINCR;
				tp = tcp_close(tp);
				goto findpcb;
			}

			if (tp->rcv_wnd == 0 && ti->ti_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_rcvwinprobe++;
			} else
				goto dropafterack;
		} else
			tcpstat.tcps_rcvbyteafterwin += todrop;
		m_adj(m, -todrop);
		ti->ti_len -= todrop;
		tiflags &= ~(TH_PUSH | TH_FIN);
	}

	//TODO:TSTAMP
	if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
			SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len + ((tiflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = ts_val;
	}

	if (tiflags & TH_RST) {
		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
			so->so_error = -ECONNREFUSED;
			goto close;
		case TCPS_ESTABLISHED:
		case TCPS_FIN_WAIT_1:
		case TCPS_FIN_WAIT_2:
		case TCPS_CLOSE_WAIT:
			so->so_error = -ECONNRESET;
			close:
			tp->t_state =  TCPS_CLOSED;
			tcpstat.tcps_drops++;
			tp = tcp_close(tp);
			goto drop;
		case TCPS_CLOSING:
		case TCPS_LAST_ACK:
		case TCPS_TIME_WAIT:
			tp = tcp_close(tp);
			goto drop;
		}
	}


	if(tiflags & TH_SYN) {
		tp = tcp_drop(tp, -ECONNRESET);
		goto dropwithreset;
	}


	if ((tiflags & TH_ACK) == 0)
		goto drop;

	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
		if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
				SEQ_GT(ti->ti_ack, tp->snd_max)	)
			goto dropwithreset;
		tcpstat.tcps_connects++;
		soisconnected(so);
		tp->t_state = TCPS_ESTABLISHED;
		if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE | TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}

		tcp_reass(tp, NULL, NULL);
		tp->snd_wl1 = ti->ti_seq -1;
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) {
			if (ti->ti_len == 0 && tiwin == tp->snd_wnd) {
				tcpstat.tcps_rcvdupack++;
				if (tp->t_timer[TCPT_REXMT] == 0 ||
						ti->ti_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks == tcprexmtthresh) {
					u32	onxt = tp->snd_nxt;
					u32 win1 = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
					if (win1 < 2)
						win1 = 2;
					tp->snd_ssthresh = win1 * tp->t_maxseg;
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_rtt = 0;
					tp->snd_nxt = ti->ti_ack;
					tp->snd_cwnd = tp->t_maxseg;
					tcp_output(tp);
					tp->snd_cwnd = tp->snd_ssthresh + tp->t_maxseg * tp->t_dupacks;
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;

				} else if (tp->t_dupacks > tcprexmtthresh) {
					tp->snd_cwnd += tp->t_maxseg;
					tcp_output(tp);
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}
		if (tp->t_dupacks > tcprexmtthresh &&
				tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;

		if (SEQ_GT(ti->ti_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack;
		}

		acked = ti->ti_ack - tp->snd_una;
		tcpstat.tcps_rcvackpack++;
		tcpstat.tcps_rcvackbyte += acked;

		if (ts_present)
			tcp_xmit_timer(tp, tcp_now - ts_ecr + 1);
		else if (tp->t_rtt && SEQ_GT(ti->ti_ack, tp->t_rtseq))
			tcp_xmit_timer(tp, tp->t_rtt);

		if (ti->ti_ack == tp->snd_max){
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

		{
			u32 cw = tp->snd_cwnd;
			u32 incr = tp->t_maxseg;

			if (cw > tp->snd_ssthresh)
				incr = incr * incr / cw + incr / 8;
			tp->snd_cwnd = min(cw + incr, TCP_MAXWIN << tp->snd_scale);
		}

		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;

		}
		if (so->so_snd.sb_flags & SB_NOTIFY)
			sowwakeup(so);
		tp->snd_una = ti->ti_ack;

		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				if (so->so_state & SS_CANTRCVMORE) {
					soisdisconnected(so);
					tp->t_timer[TCPT_2MSL] = tcp_maxidle;
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

		case TCPS_CLOSING:
			if (ourfinisacked) {
				tp->t_state = TCPS_TIME_WAIT;
				tcp_canceltimers(tp);
				tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
				soisdisconnected(so);
			}
			break;
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			goto dropafterack;
		}
	}

	step6:
	if ((tiflags & TH_ACK) &&
			(SEQ_LT(tp->snd_wl1, ti->ti_seq) || (tp->snd_wl1 == ti->ti_seq &&
					(SEQ_LT(tp->snd_wl2, ti->ti_ack) ||
							(tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd))))) {
		if (ti->ti_len == 0 && tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd)
			tcpstat.tcps_rcvwinupd++;

		tp->snd_wnd = tiwin;
		tp->snd_wl1 = ti->ti_seq;
		tp->snd_wl2 = ti->ti_ack;

		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;

	}

	if ((tiflags & TH_URG) && ti->ti_urp &&
			TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		if (ti->ti_urp + so->so_rcv.sb_cc > sb_max) {
			 ti->ti_urp = 0;
			 tiflags &= ~ TH_URG;
			 goto dodata;
		}

		if (SEQ_GT(ti->ti_seq + ti->ti_urp, tp->rcv_up)) {
			tp->rcv_up = ti->ti_seq + ti->ti_urp;
			so->so_oobmark = so->so_rcv.sb_cc + (tp->rcv_up - tp->rcv_nxt) - 1;

			if (so->so_oobmark == 0)
				so->so_state |= SS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		if (ti->ti_urp <= ti->ti_len
#ifdef SO_OOBINLINE
				&& (so->so_options & SO_OOBINLINE) == 0
#endif
		)
			tcp_pulloutofband(so, ti, m);
	} else {
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
	}


	dodata:
	if ((ti->ti_len || (tiflags & TH_FIN)) &&
			TCPS_HAVERCVDFIN(tp->t_state) == 0 ) {

		TCP_REASS(tp, ti, m, so, tiflags);

		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);

	} else {
		m_freem(m);
		tiflags &= ~TH_FIN;
	}

	if (tiflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}

		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;
		case TCPS_FIN_WAIT_2:
			tp->t_state = TCPS_TIME_WAIT;
			tcp_canceltimers(tp);
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			soisdisconnected(so);
			break;
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			break;
		}
	}

	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, &tcp_saveti, 0);

	if (needoutput || (tp->t_flags & TF_ACKNOW))
		tcp_output(tp);
	return;
	dropafterack:
	if (tiflags & TH_RST)
		goto drop;
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	tcp_output(tp);
	return;

	dropwithreset:
	if ((tiflags & TH_RST) || (m->m_flags & (M_BCAST | M_MCAST)) || IN_MULTICAST(ntohl(ti->ti_dst.s_addr)))
		goto drop;

	if (tiflags & TH_ACK)
		tcp_respond(tp, ti, m, 0, ti->ti_ack, TH_RST);
	else {
		if (tiflags & TH_SYN)
			ti->ti_len++;
		tcp_respond(tp, ti, m, ti->ti_seq + ti->ti_len, 0, TH_RST| TH_ACK);
	}

	if (dropsocket)
		soabort(so);
	return;

	drop:
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, &tcp_saveti, 0);
	m_freem(m);
	if (dropsocket)
		soabort(so);
	return;
}









