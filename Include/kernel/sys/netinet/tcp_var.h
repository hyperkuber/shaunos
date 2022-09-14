/*
 * tcp_val.h
 *
 *  Created on: Sep 21, 2013
 *      Author: root
 */

#ifndef TCP_VAL_H_
#define TCP_VAL_H_


#include <kernel/sys/netinet/tcpip.h>
#include <kernel/sys/netinet/in_pcb.h>



#define	TCPT_NTIMERS	4

struct tcpcb {
	struct tcpiphdr *seg_next;
	struct tcpiphdr *seg_prev;
	s16	t_state;
	s16	t_timer[TCPT_NTIMERS];
	s16	t_rxtshift;
	s16	t_rxtcur;
	s16	t_dupacks;
	u16	t_maxseg;
	s8	t_force;
	u16	t_flags;
	struct tcpiphdr *t_template;
	struct inpcb *t_inpcb;
	struct mbuf *t_tempbuf;
	u32	snd_una;
	u32	snd_nxt;
	u32	snd_up;
	u32	snd_wl1;
	u32	snd_wl2;
	u32	iss;
	u32	snd_wnd;
	u32	rcv_wnd;
	u32	rcv_nxt;
	u32	rcv_up;
	u32	irs;
	u32	rcv_adv;
	u32	snd_max;
	u32	snd_cwnd;
	u32	snd_ssthresh;
	s16	t_idle;
	s16	t_rtt;
	u32	t_rtseq;
	s16	t_srtt;
	s16	t_rttvar;
	u16	t_rttmin;
	u32	max_sndwnd;
	s8	t_oobflags;
	s8	t_iobc;
	s16	t_softerror;
	u8	snd_scale;
	u8	rcv_scale;
	u8	request_r_scale;
	u8	requested_s_scale;

	u32	ts_recent;
	u32	ts_recent_age;
	u32	last_ack_sent;
};


#define TF_ACKNOW	0x0001
#define TF_DELACK	0x0002
#define TF_NODELAY	0x0004
#define TF_NOOPT	0x0008
#define TF_SENTFIN	0x0010
#define TF_REQ_SCALE	0x0020
#define TF_RCVD_SCALE	0x0040
#define TF_REQ_TSTMP	0x0080
#define TF_RCVD_TSTMP	0x0100
#define TF_SACK_PERMIT	0x0200

#define TCPOOB_HAVEDATA	0x01
#define TCPOOB_HADDATA	0x02

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))



#define TCP_RTT_SCALE	8
#define TCP_RTT_SHIFT	3
#define TCP_RTTVAR_SCALE	4
#define TCP_RTTVAR_SHIFT	2


/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	(((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */
#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))


/*
* TCP statistics.
* Many of these should be kept per connection,
* but that's inconvenient at the moment.
*/
struct	tcpstat {
	u32	tcps_connattempt;	/* connections initiated */
	u32	tcps_accepts;		/* connections accepted */
	u32	tcps_connects;		/* connections established */
	u32	tcps_drops;		/* connections dropped */
	u32	tcps_conndrops;		/* embryonic connections dropped */
	u32	tcps_closed;		/* conn. closed (includes drops) */
	u32	tcps_segstimed;		/* segs where we tried to get rtt */
	u32	tcps_rttupdated;	/* times we succeeded */
	u32	tcps_delack;		/* delayed acks sent */
	u32	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u32	tcps_rexmttimeo;	/* retransmit timeouts */
	u32	tcps_persisttimeo;	/* persist timeouts */
	u32	tcps_keeptimeo;		/* keepalive timeouts */
	u32	tcps_keepprobe;		/* keepalive probes sent */
	u32	tcps_keepdrops;		/* connections dropped in keepalive */

	u32	tcps_sndtotal;		/* total packets sent */
	u32	tcps_sndpack;		/* data packets sent */
	u32	tcps_sndbyte;		/* data bytes sent */
	u32	tcps_sndrexmitpack;	/* data packets retransmitted */
	u32	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u32	tcps_sndacks;		/* ack-only packets sent */
	u32	tcps_sndprobe;		/* window probes sent */
	u32	tcps_sndurg;		/* packets sent with URG only */
	u32	tcps_sndwinup;		/* window update-only packets sent */
	u32	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u32	tcps_rcvtotal;		/* total packets received */
	u32	tcps_rcvpack;		/* packets received in sequence */
	u32	tcps_rcvbyte;		/* bytes received in sequence */
	u32	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u32	tcps_rcvbadoff;		/* packets received with bad offset */
	u32	tcps_rcvshort;		/* packets received too short */
	u32	tcps_rcvduppack;	/* duplicate-only packets received */
	u32	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u32	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u32	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u32	tcps_rcvoopack;		/* out-of-order packets received */
	u32	tcps_rcvoobyte;		/* out-of-order bytes received */
	u32	tcps_rcvpackafterwin;	/* packets with data after window */
	u32	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u32	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u32	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u32	tcps_rcvdupack;		/* rcvd duplicate acks */
	u32	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u32	tcps_rcvackpack;	/* rcvd ack packets */
	u32	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u32	tcps_rcvwinupd;		/* rcvd window update packets */
	u32	tcps_pawsdrop;		/* segments dropped due to PAWS */
	u32	tcps_predack;		/* times hdr predict ok for acks */
	u32	tcps_preddat;		/* times hdr predict ok for data pkts */
	u32	tcps_pcbcachemiss;
	u32	tcps_persistdrop;	/* timeout in persist state */
	u32	tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
};


#define	TA_INPUT 	0
#define	TA_OUTPUT	1
#define	TA_USER		2
#define	TA_RESPOND	3
#define	TA_DROP		4

extern struct inpcb tcb;
extern struct tcpstat	tcpstat;
extern u32	tcp_now;
extern struct	inpcb *tcp_last_inpcb;


extern int
tcp_output(struct tcpcb *tp);
extern void
tcp_input(struct mbuf *m, int iphlen);
extern struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno);
extern struct tcpcb *
tcp_close(struct tcpcb *tp);
extern void
tcp_trace(short act, short ostate, struct tcpcb *tp, struct tcpiphdr *ti, int req);
extern void
tcp_respond(struct tcpcb *tp, struct tcpiphdr *ti,
		struct mbuf *m, u32 ack, u32 seq, int flags);
extern struct tcpiphdr *
tcp_template(struct tcpcb *tp);
extern int
tcp_mss(struct tcpcb *tp, int offer);
extern void
tcp_quench(struct inpcb *inp, int errno);
extern void
tcp_setpersist(struct tcpcb *tp);
extern int
tcp_usrreq(struct socket *so, int req,
		struct mbuf *m, struct mbuf *nam, struct mbuf *control);
extern struct tcpcb *
tcp_usrclosed(struct tcpcb *tp);
extern struct tcpcb *
tcp_disconnect(struct tcpcb *tp);
extern int
tcp_attach(struct socket *so);
extern int
tcp_usrreq(struct socket *so, int req,
		struct mbuf *m, struct mbuf *nam, struct mbuf *control);
extern struct tcpcb *
tcp_newtcpcb(struct inpcb *inp);
extern void
tcp_drain();
extern void
tcp_init();
extern void
tcp_fasttimo();
extern void
tcp_fasttimo();


#endif /* TCP_VAL_H_ */


