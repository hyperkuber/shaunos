/*
 * tcp_output.c
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
#include <string.h>
#include <kernel/sys/netinet/tcp_fsm.h>
#include <kernel/sys/socketval.h>

#define MAX_TCPOPTLEN	32	/* max # bytes that go in options */


extern int in_cksum(struct mbuf *m, int len);
/*
 * Calculate rto
 * 						t_srtt
 * 						------- + t_rttvar
 * 						4
 * rto = srtt+2*rttvar = ---------------
 * 								2
 *
 */
void
tcp_setpersist(struct tcpcb *tp)
{
	int t;
	t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

	if (tp->t_timer[TCPT_REXMT])
		LOG_ERROR("REXMT");

	TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
			t * tcp_backoff[tp->t_rxtshift],
			TCPTV_PERSMIN, TCPTV_PERSMAX);

	if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
		tp->t_rxtshift++;
}

int tcp_output(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	long len, win;
	int off, flags, error;
	struct mbuf *m;
	struct tcpiphdr *ti;
	u8	opt[MAX_TCPOPTLEN];
	unsigned optlen, hdrlen;
	int idle, sendalot;

	idle = (tp->snd_max == tp->snd_una);

	if (idle && tp->t_idle >= tp->t_rxtcur){
		tp->snd_cwnd = tp->t_maxseg;
	}

again:
	sendalot = 0;

	off = tp->snd_nxt - tp->snd_una;
	win = min(tp->snd_wnd, tp->snd_cwnd);
	flags = tcp_outflags[tp->t_state];

	if (tp->t_force) {
		if (win == 0) {
			if (off < so->so_snd.sb_cc)
				flags &= ~TH_FIN;
			win = 1;
		} else {
			tp->t_timer[TCPT_PERSIST] = 0;
			tp->t_rxtshift = 0;
		}
	}

	len = min(so->so_snd.sb_cc, win) - off;
	if (len < 0) {
		len = 0;
		if (win == 0) {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->snd_nxt = tp->snd_una;
		}
	}

	if (len > tp->t_maxseg) {
		len = tp->t_maxseg;
		sendalot = 1;
	}

	if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
		flags &= ~TH_FIN;

	win = sbspace(&so->so_rcv);

	if (len) {
		if (len == tp->t_maxseg)
			goto send;
		if ((idle || (tp->t_flags & TF_NODELAY)) && (len + off) >= so->so_snd.sb_cc)
			goto send;

		if(tp->t_force)
			goto send;
		if (len >= tp->max_sndwnd / 2)
			goto send;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))
			goto send;
	}

	/*
	 * window update
	 */
	if (win > 0) {
		long adv = min(win, (long)TCP_MAXWIN << tp->rcv_scale) -
				(tp->rcv_adv - tp->rcv_nxt);

		if (adv >= (long)(2 * tp->t_maxseg))
			goto send;
		if (2 * adv >= (long)so->so_rcv.sb_hiwat)
			goto send;
	}

	if (tp->t_flags & TF_ACKNOW)
		goto send;
	if (flags & (TH_SYN | TH_RST))
		goto send;

	if ((flags & TH_FIN) && ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
		goto send;

	if(so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 && tp->t_timer[TCPT_PERSIST] == 0) {
		tp->t_rxtshift = 0;
		tcp_setpersist(tp);
	}

	return 0;
send:
	optlen = 0;
	hdrlen = sizeof(struct tcpiphdr);
	if (flags & TH_SYN) {
		tp->snd_nxt = tp->iss;
		if ((tp->t_flags & TF_NOOPT) == 0) {
			u16	mss;
			opt[0] = TCPOPT_MAXSEG;
			opt[1] = 4;
			mss = htons((u16)tcp_mss(tp, 0));
			bcopy((caddr_t)&mss, (opt+2), sizeof(mss));
			optlen = 4;

			if ((tp->t_flags & TF_REQ_SCALE) && ((flags & TH_ACK) == 0 || (tp->t_flags & TF_RCVD_SCALE))) {
				*(u32 *)(opt+optlen) = htonl(TCPOPT_NOP << 24 | TCPOPT_WINDOW << 16 |
						TCPOLEN_WINDOW << 8 | tp->request_r_scale);

				optlen += 4;
			}
		}
	}

	if ((tp->t_flags & (TF_REQ_TSTMP | TF_NOOPT)) == TF_REQ_TSTMP && (flags & TH_RST) == 0 &&
			((flags & (TH_SYN | TH_ACK)) == TH_SYN || (tp->t_flags & TF_RCVD_TSTMP))) {
		u32	*lp = (u32 *) (opt+optlen);
		*lp++ = htonl(TCPOPT_TSTAMP_HDR);
		*lp++ = htonl(tcp_now);
		*lp++ = htonl(tp->ts_recent);
		optlen += TCPOLEN_TSTAMP_APPA;
	}

	hdrlen += optlen;

	if (len) {
		if (tp->t_force && len == 1)
			tcpstat.tcps_sndprobe++;
		else if(SEQ_LT(tp->snd_nxt, tp->snd_max)) {
			tcpstat.tcps_sndrexmitpack++;
			tcpstat.tcps_sndrexmitbyte += len;
		} else {
			tcpstat.tcps_sndpack++;
			tcpstat.tcps_sndbyte += len;
		}

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = -ENOBUFS;
			goto out;
		}

		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
		if (len <= MHLEN - hdrlen - max_linkhdr) {
			m_copydata(so->so_snd.sb_mb, off, len, mtod(m, caddr_t) + hdrlen);
			m->m_len += len;
		} else {
			m->m_next = m_copy(so->so_snd.sb_mb, off, len);
			if (m->m_next == NULL) {
				len = 0;
				(void) m_free(m);
				error = -ENOBUFS;
				goto out;
			}

		}

		if (off +len == so->so_snd.sb_cc)
			flags |= TH_PUSH;
	} else {
		if (tp->t_flags & TF_ACKNOW)
			tcpstat.tcps_sndacks++;
		else if (flags & (TH_SYN | TH_FIN | TH_RST))
			tcpstat.tcps_sndctrl++;
		else if (SEQ_GT(tp->snd_up, tp->snd_una))
			tcpstat.tcps_sndurg++;
		else
			tcpstat.tcps_sndwinup++;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = -ENOBUFS;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
	}
	m->m_pkthdr.rcvif = NULL;
	ti = mtod(m, struct tcpiphdr *);
	if (tp->t_template == NULL)
		LOG_INFO("tcp templete is 0");

	bcopy(tp->t_template, ti, sizeof(struct tcpiphdr));

	if ((flags & TH_FIN) && (tp->t_flags & TF_SENTFIN) && tp->snd_nxt == tp->snd_max)
		tp->snd_nxt--;

	if (len || (flags & (TH_SYN | TH_FIN)) || tp->t_timer[TCPT_PERSIST])
		ti->ti_seq = htonl(tp->snd_nxt);
	else
		ti->ti_seq = htonl(tp->snd_max);

	ti->ti_ack = htonl(tp->rcv_nxt);

	if (optlen) {
		bcopy(opt, (ti+1), optlen);
		ti->ti_off = (sizeof(struct tcphdr) + optlen) >> 2;
	}

	ti->ti_flags = flags;

	if (win < (long)(so->so_rcv.sb_hiwat / 4) && win < (long)tp->t_maxseg)
		win = 0;
	if (win > (long)TCP_MAXWIN << tp->rcv_scale)
		win = (long)TCP_MAXWIN << tp->rcv_scale;

	if (win < (long)(tp->rcv_adv - tp->rcv_nxt))
		win = (long)(tp->rcv_adv - tp->rcv_nxt);
	ti->ti_win = htons((u16) (win >> tp->rcv_scale));
	if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
		ti->ti_urp = htons((u16)(tp->snd_up - tp->snd_nxt));
		ti->ti_flags |= TH_URG;
	} else
		tp->snd_up = tp->snd_una;

	if (len + optlen)
		ti->ti_len = htons((u16)(sizeof(struct tcphdr) + optlen + len));

	ti->ti_sum = in_cksum(m, (int)(hdrlen+len));

	if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0)	{
		u32	startseq = tp->snd_nxt;

		if (flags & (TH_SYN | TH_FIN)) {
			if (flags & TH_SYN)
				tp->snd_nxt++;
			if (flags & TH_FIN) {
				tp->snd_nxt++;
				tp->t_flags |= TF_SENTFIN;
			}
		}

		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
			tp->snd_max = tp->snd_nxt;

			if (tp->t_rtt == 0) {
				tp->t_rtt = 1;
				tp->t_rtseq = startseq;
				tcpstat.tcps_segstimed++;
			}
		}

		if (tp->t_timer[TCPT_REXMT] == 0 &&
				tp->snd_nxt != tp->snd_una ) {
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

			if(tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}
	} else if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
		tp->snd_max = tp->snd_nxt + len;

	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_OUTPUT, tp->t_state, tp, ti, 0);

	m->m_pkthdr.len = hdrlen + len;
	((struct ip *)ti)->ip_len = m->m_pkthdr.len;
	((struct ip *)ti)->ip_ttl = tp->t_inpcb->inp_ip.ip_ttl;
	((struct ip *)ti)->ip_tos = tp->t_inpcb->inp_ip.ip_tos;

	error = ip_output(m, tp->t_inpcb->inp_options, &tp->t_inpcb->inp_route,
			so->so_options & SO_DONTROUTE, 0);

	if (error) {
		out:
			if (error == -ENOBUFS) {
				tcp_quench(tp->t_inpcb, 0);
				return 0;
			}

			if ((error == -EHOSTUNREACH || error == -ENETDOWN) &&
					TCPS_HAVERCVDSYN(tp->t_state)) {
				tp->t_softerror = error;
				return 0;
			}

			return error;
	}
	tcpstat.tcps_sndtotal++;
	if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + win;
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);

	if(sendalot)
		goto again;
	return 0;
}



























