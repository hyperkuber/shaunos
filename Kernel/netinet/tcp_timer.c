/*
 * tcp_timer.c
 *
 *  Created on: Sep 22, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/sys/netinet/tcp_fsm.h>
#include <kernel/sys/netinet/tcp_timer.h>
#include <kernel/sys/netinet/tcp_seq.h>
#include <kernel/sys/netinet/tcp_var.h>


int	tcp_keepidle = TCPTV_KEEP_IDLE;
int	tcp_keepintvl = TCPTV_KEEPINTVL;
int	tcp_keepcnt = TCPTV_KEEPCNT;		/* max idle probes */
int	tcp_maxpersistidle = TCPTV_KEEP_IDLE;	/* max idle time in persist */
int	tcp_maxidle;


int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

void
tcp_canceltimers(struct tcpcb *tp)
{
	int i;
	for(i=0; i<TCPT_NTIMERS; i++)
		tp->t_timer[i] = 0;
}


void
tcp_fasttimo()
{
	struct inpcb *inp;
	struct tcpcb *tp;
	//LOG_INFO("tcp fast time out");
	inp = tcb.inp_next;
	if (inp){
		for(; inp != &tcb; inp = inp->inp_next)
			if ((tp = (struct tcpcb *)inp->inp_ppcb) && (tp->t_flags & TF_DELACK)) {
				tp->t_flags &= ~TF_DELACK;
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_delack++;
				tcp_output(tp);
			}
	}
}


void
tcp_slowtimo()
{
	struct inpcb *inp, *inpnext;
	struct tcpcb *tp;
	int i;

	tcp_maxidle = TCPTV_KEEPCNT * tcp_keepintvl;
	//LOG_INFO("tcp slow time out");
	inp = tcb.inp_next;
	if (inp == NULL)
		return;

	for (; inp != &tcb; inp = inpnext){
		inpnext = inp->inp_next;
		tp = intotcpcb(inp);
		if (tp == NULL)
			continue;

		for (i=0; i<TCPT_NTIMERS; i++){
			if (tp->t_timer[i] && --tp->t_timer[i] == 0){
				tcp_usrreq(tp->t_inpcb->inp_socket, PRU_SLOWTIMO, NULL, (struct mbuf *)i, NULL);
				if (inpnext->inp_prev != inp)
					goto tpgone;
			}
		}

		tp->t_idle++;
		if (tp->t_rtt)
			tp->t_rtt++;
		tpgone:
		;
	}

	tcp_iss += TCP_ISSINCR / PR_SLOWHZ;
	tcp_now++;
}

struct tcpcb *
tcp_timers(struct tcpcb *tp, int timer)
{

	int rexmt;
	int win;
	switch (timer){
	case TCPT_2MSL:
		if (tp->t_state != TCPS_TIME_WAIT &&
				tp->t_idle <= tcp_maxidle)
			tp->t_timer[TCPT_2MSL] = tcp_keepintvl;
		else
			tp = tcp_close(tp);

		break;
	case TCPT_PERSIST:
		tcpstat.tcps_persisttimeo++;
		tcp_setpersist(tp);
		tp->t_force = 1;
		tcp_output(tp);
		tp->t_force = 0;
		break;

	case TCPT_KEEP:
		tcpstat.tcps_keeptimeo++;
		if (tp->t_state < TCPS_ESTABLISHED)
			goto dropit;

		if ((tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE) &&
				tp->t_state <= TCPS_CLOSE_WAIT) {
			if (tp->t_idle >= tcp_keepidle + tcp_maxidle)
				goto dropit;

			tcpstat.tcps_keepprobe++;
			tcp_respond(tp, tp->t_template, NULL, tp->rcv_nxt, tp->snd_una -1, 0);

			tp->t_timer[TCPT_KEEP] = tcp_keepintvl;

		} else
			tp->t_timer[TCPT_KEEP] = tcp_keepidle;
		break;
		dropit:
		tcpstat.tcps_keepdrops++;
		tp = tcp_drop(tp, -ETIMEDOUT);
		break;
	case TCPT_REXMT:
		if(++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
			tp->t_rxtshift = TCP_MAXRXTSHIFT;
			tcpstat.tcps_timeoutdrop++;
			tp = tcp_drop(tp, tp->t_softerror ? tp->t_softerror : -ETIMEDOUT);
			break;
		}

		tcpstat.tcps_rexmttimeo++;
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
		TCPT_RANGESET(tp->t_rxtcur, rexmt, tp->t_rttmin, TCPTV_REXMTMAX);
		tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

		if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
			in_losing(tp->t_inpcb);
			tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
			tp->t_srtt = 0;
		}
		tp->snd_nxt = tp->snd_una;
		tp->t_rtt = 0;

		win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
		if (win < 2)
			win = 2;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_ssthresh = win * tp->t_maxseg;
		tp->t_dupacks = 0;

		tcp_output(tp);
		break;
	}
	return tp;
}






