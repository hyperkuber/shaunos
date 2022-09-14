/*
 * mbuf.h
 *
 *  Created on: Jun 13, 2013
 *      Author: root
 */

#ifndef SHAUN_MBUF_H_
#define SHAUN_MBUF_H_


#include <kernel/types.h>



#define MCLBYTES	2048
#define MHLEN		100
#define MINCLSIZE	208
#define	MLEN		108
#define	MSIZE		128
#define	MAXMCLBYTES	(64 * 1024)		/* largest cluster from the stack */



struct mbuf;
struct ifnet;

struct m_hdr {
	struct mbuf *mh_next;
	struct mbuf *mh_nextpkt;
	int mh_len;				//data length in this mbuf
	caddr_t mh_data;		//point to data
	short mh_type;
	short mh_flags;
};

struct pkthdr {
	int len;			//total packet length
	struct ifnet *rcvif;//receive interface
};


struct m_ext {
	caddr_t ext_buf;
	void (*ext_free)();
	u32	ext_refcnt;
};

struct mbuf {
	struct m_hdr m_hdr;
	union {
		struct {
			struct pkthdr MH_pkthdr;
			union {
				struct m_ext MH_ext;
				char MH_databuf[MHLEN];
			} MH_dat;
		} MH;

		char M_databuf[MLEN];
	} M_dat;
};

#define m_next	m_hdr.mh_next
#define m_len	m_hdr.mh_len
#define m_data	m_hdr.mh_data
#define	m_type	m_hdr.mh_type
#define m_flags	m_hdr.mh_flags
#define m_nextpkt	m_hdr.mh_nextpkt
#define m_act	m_nextpkt;
#define m_pkthdr	M_dat.MH.MH_pkthdr
#define m_ext	M_dat.MH.MH_dat.MH_ext
#define m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define m_dat	M_dat.M_databuf
#define m_extrefcnt	M_dat.MH.MH_dat.MH_ext.ext_refcnt

//m_flags
#define M_BCAST	(1<<0)
#define M_EOR	(1<<1)
#define M_EXT	(1<<2)
#define M_MCAST	(1<<3)
#define M_PKTHDR	(1<<4)
#define M_CLUSTER	(1<<5)
#define M_COPYFLAGS	(M_PKTHDR|M_EOR|M_BCAST|M_MCAST)

//mt_type
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#define	MT_SONAME	8	/* socket name */
#define	MT_SOOPTS	10	/* socket options */
#define	MT_FTABLE	11	/* fragment reassembly header */
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* interface address */
#define MT_CONTROL	14	/* extra-data protocol message */
#define MT_OOBDATA	15	/* expedited data  */
#define MT_TAG          16      /* volatile metadata associated to pkts */
#define MT_MAX		32	/* enough? */


#define mtod(m,t)	((t)(m)->m_data)






/* length to m_copy to copy all */
#define	M_COPYALL	1000000000


#define M_NOWAIT	0
#define M_WAITOK	1

#define	M_DONTWAIT	M_NOWAIT
#define	M_WAIT		M_WAITOK


#define MCLGET(m, how) m = m_clget((m), (how), NULL, MCLBYTES)
#define MFREE(m, n)	n = m_free((m))
#define MGET(m, how, type)	m = m_get((how), (type))
#define MGETHDR(m, how, type)	m = m_gethdr((how), (type))
/*
 * Set the m_data pointer of a newly-allocated mbuf (m_get/MGET) to place
 * an object of the specified size at the end of the mbuf, longword aligned.
 */
#define	M_ALIGN(m, len) \
	(m)->m_data += (MLEN - (len)) &~ (sizeof(long) - 1)
/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_MOVE_PKTHDR.
 */
#define	MH_ALIGN(m, len) \
	(m)->m_data += (MHLEN - (len)) &~ (sizeof(long) - 1)

#define	M_PREPEND(m, plen, how) \
		(m) = m_prepend((m), (plen), (how))

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 */
#define	M_LEADINGSPACE(m) m_leadingspace(m)

/*
* Move just m_pkthdr from from to to,
* remove M_PKTHDR and clean flags/tags for from.
*/
#define M_MOVE_HDR(to, from) do {					\
	(to)->m_pkthdr = (from)->m_pkthdr;				\
	(from)->m_flags &= ~M_PKTHDR;					\
} while (/* CONSTCOND */ 0)

/*
* MOVE mbuf pkthdr from from to to.
* from must have M_PKTHDR set, and to must be empty.
*/
#define	M_MOVE_PKTHDR(to, from) do {					\
	(to)->m_flags = ((to)->m_flags & (M_EXT | M_CLUSTER));		\
	(to)->m_flags |= (from)->m_flags & M_COPYFLAGS;			\
	M_MOVE_HDR((to), (from));					\
	if (((to)->m_flags & M_EXT) == 0)				\
		(to)->m_data = (to)->m_pktdat;				\
} while (0)

//#define MCLGET(m, how) (void) m_clget((m), (how), NULL, MCLBYTES)
#define MCLGETI(m, how, ifp, l) m_clget((m), (how), (ifp), (l))


#define  m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

struct mbuf *
m_devget(char *buf, int totlen, int off, struct ifnet *ifp,
    void (*copy)(char *from, caddr_t to, u32 len));

struct mbuf *m_freem(struct mbuf *m);

extern int	max_linkhdr;			/* largest link-level header */
extern int	max_protohdr;			/* largest protocol header */
extern int	max_hdr;			/* largest link+protocol header */
extern int	max_datalen;			/* MHLEN - max_hdr */

extern u32 mbuf_allocated;
extern u32	mcl_allocated;

struct mbuf*
m_copym(struct mbuf *m, int off, int len, int wait);
struct mbuf *
m_pullup(struct mbuf *n, int len);

void
m_cat(struct mbuf *m,struct mbuf *n);

void
m_adj(struct mbuf *mp, int req_len);

struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off);

struct mbuf *
m_prepend(struct mbuf *m, int len, int how);

struct mbuf *
m_clget(struct mbuf *m, int how, struct ifnet *ifp, u32 pktlen);

inline struct mbuf *
m_gethdr(int nowwait, int type);
inline struct mbuf *
m_get(int nowait, int type);
struct mbuf *
m_free(struct mbuf *m);
struct mbuf *
m_getclr(int how, short type);
void
m_copydata(struct mbuf *m, int off, int len, caddr_t cp);
#endif /* SHAUN_MBUF_H_ */
