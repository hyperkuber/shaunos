/*
 * tcp_usrreq.c
 *
 *  Created on: Oct 8, 2013
 *      Author: Shaun Yuan
 */


#include <kernel/kernel.h>
#include <kernel/sys/netinet/in_pcb.h>
#include <kernel/sys/netinet/tcp.h>
#include <kernel/sys/netinet/tcp_var.h>
#include <kernel/sys/netinet/tcp_seq.h>
#include <kernel/sys/netinet/tcp_timer.h>
#include <kernel/sys/netinet/ip_val.h>
#include <kernel/mbuf.h>
#include <kernel/stat.h>
#include <string.h>
#include <kernel/sys/netinet/tcp_fsm.h>
#include <kernel/sys/socketval.h>
#include <kernel/sys/socket.h>


u32	tcp_sendspace = 1024*8;
u32	tcp_recvspace = 1024*8;

int
tcp_usrreq(struct socket *so, int req,
		struct mbuf *m, struct mbuf *nam, struct mbuf *control)
{
	struct inpcb *inp;
	struct tcpcb *tp;

	int error = 0;
	int ostate;

	if (req == PRU_CONTROL)
		return (in_control(so, (int)m, (caddr_t)nam, (struct ifnet *)control));

	if (control && control->m_len) {
		m_freem(control);

		if (m)
			m_freem(m);
		return -EINVAL;

	}

	inp = sotoinpcb(so);
	if (inp == NULL && req != PRU_ATTACH) {
		return -EINVAL;
	}

	if (inp) {
		tp = intotcpcb(inp);
		ostate = tp->t_state;
	} else
		ostate = 0;

	switch (req) {
	case PRU_ATTACH:
		if (inp) {
			error = -EISCONN;
			break;
		}

		error = tcp_attach(so);
		if (error)
			break;
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME;
		tp = (struct tcpcb *)((struct inpcb *)(so)->so_pcb)->inp_ppcb;
		break;
	case PRU_DETACH:
		if (tp->t_state > TCPS_LISTEN)
			tp = tcp_disconnect(tp);
		else
			tp = tcp_close(tp);
		break;
	case PRU_BIND:
		error = in_pcbbind(inp, nam);
		if (error)
			break;
		break;
	case PRU_LISTEN:
		if (inp->inp_lport == 0)
			error = in_pcbbind(inp, NULL);
		if (error == 0)
			tp->t_state = TCPS_LISTEN;
		break;
	case PRU_CONNECT:
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, NULL);
			if (error)
				break;
		}
		error = in_pcbconnect(inp, nam);
		if (error)
			break;
		tp->t_template = tcp_template(tp);
		if (tp->t_template == NULL) {
			in_pcbdisconnect(inp);
			error = -ENOBUFS;
			break;
		}

		while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
				(TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
			tp->request_r_scale++;

		soisconnecting(so);
		tcpstat.tcps_connattempt++;
		tp->t_state = TCPS_SYN_SENT;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;

		tp->iss = tcp_iss;
		tcp_iss += TCP_ISSINCR / 2;
		tcp_sendseqinit(tp);
		error = tcp_output(tp);
		break;
	case PRU_CONNECT2:
		error = -EOPNOTSUPP;
		break;
	case PRU_DISCONNECT:
		tp = tcp_disconnect(tp);
		break;
	case PRU_ACCEPT:
		in_setpeeraddr(inp, nam);
		break;
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp);
		break;
	case PRU_RCVD:
		tcp_output(tp);
		break;
	case PRU_SEND:
		sbappend(&so->so_snd, m);
		error = tcp_output(tp);
		break;
	case PRU_ABORT:
		tp = tcp_drop(tp, -ECONNABORTED);
		break;
	case PRU_SENSE:	//fstat
		((struct stat *)m)->st_blksize = (blksize_t)so->so_snd.sb_hiwat;
		return 0;
	case PRU_RCVOOB:
		if ((so->so_oobmark == 0 && (so->so_state & SS_RCVATMARK) == 0) ||
				(so->so_options & SO_OOBINLINE) ||
				(tp->t_oobflags & TCPOOB_HADDATA)) {
			error = -EINVAL;
			break;
		}
		if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0){
			error = -EWOULDBLOCK;
			break;
		}
		m->m_len = 1;
		*mtod(m, caddr_t) = tp->t_iobc;
		if (((int) nam & MSG_PEEK) == 0)
			tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		break;

	case PRU_SENDOOB:
		if (sbspace(&so->so_snd) < -512) {
			m_freem(m);
			error = -ENOBUFS;
			break;
		}
		sbappend(&so->so_snd, m);
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;

		tp->t_force = 1;
		error = tcp_output(tp);
		tp->t_force = 0;
		break;
	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;
	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;
	case PRU_SLOWTIMO:
		tp = tcp_timers(tp, (int)nam);
		req |= (int)nam << 8;
		break;
	default:
		panic("unknow tcp user request");
	}


	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, req);

	return error;
}

int tcp_attach(struct socket *so)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return error;
	}

	error = in_pcballoc(so, &tcb);
	if (error)
		return error;
	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp);
	if (tp == NULL) {
		int nofd = so->so_state & SS_NOFDREF;
		so->so_state &= ~SS_NOFDREF;
		in_pcbdetach(inp);
		so->so_state |= nofd;
		return -ENOBUFS;
	}
	tp->t_state = TCPS_CLOSED;
	return 0;
}

struct tcpcb *
tcp_disconnect(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (tp->t_state < TCPS_ESTABLISHED)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			tcp_output(tp);

	}

	return tp;
}

struct tcpcb *
tcp_usrclosed(struct tcpcb *tp)
{
	switch(tp->t_state) {
	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;
	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;
	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}

	if (tp && tp->t_state >= TCPS_FIN_WAIT_2)
		soisdisconnected(tp->t_inpcb->inp_socket);
	return tp;
}





