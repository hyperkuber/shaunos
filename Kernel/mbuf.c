/*
 * mbuf.c
 *
 *  Created on: Jun 13, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/mbuf.h>
#include <kernel/malloc.h>
#include <kernel/mm.h>
#include <kernel/kernel.h>
#include <string.h>
#include <kernel/assert.h>

u32 mbuf_allocated;
u32	mcl_allocated;

int	max_linkhdr;			/* largest link-level header */
int	max_protohdr;			/* largest protocol header */
int	max_hdr;			/* largest link+protocol header */
int	max_datalen;			/* MHLEN - max_hdr */

#define MBUF_INIT(m, pkthdr, type) {		\
	(m)->m_next = (m)->m_nextpkt = NULL;	\
	(m)->m_len = 0;							\
	(m)->m_type = type;						\
	if (pkthdr)	{							\
		(m)->m_data = (m)->m_pktdat;		\
		(m)->m_flags = M_PKTHDR;			\
	} else {								\
		(m)->m_data = (m)->m_dat;			\
	}										\
}

#define MEXT_INIT(m, buf, size, free, flag) {	\
	(m)->m_data = (m)->m_ext.ext_buf = (buf);	\
	(m)->m_flags = (flag | M_EXT);				\
	(m)->m_ext.ext_size = size;					\
	(m)->m_ext.ext_free = free;					\
}

#define MBUF_CL_INIT(m, buf, size, free, flag)	\
		MEXT_INIT(m, buf, MCLBYTES, free, flag)

//#define MALLOC(p, t, s) p = (t)kmalloc((s))
//#define MFREE(p)	kfree((p));

#define MCLFREE(p)	\
	{ \
		kfree((p));	\
		mcl_allocated--;	\
	}






 inline struct mbuf *
m_get(int nowait, int type)
{
	struct mbuf *m;
	m = (struct mbuf *)kzalloc(MSIZE);
	if (!m)
		return NULL;
	mbuf_allocated++;
	MBUF_INIT(m, 0, type);
	return m;
}

 inline struct mbuf *
m_gethdr(int nowwait, int type)
{
	struct mbuf *m;
	m = (struct mbuf *)kzalloc(MSIZE);
	if (!m)
		return NULL;
	mbuf_allocated++;
	MBUF_INIT(m, 1, type);
	return m;
}


struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n;
	if (m->m_flags & M_EXT) {
		if (--m->m_extrefcnt == 0)
			MCLFREE((m)->m_ext.ext_buf);

	}
	n = m->m_next;
	kfree((m));
	mbuf_allocated--;
	return (n);
}

struct mbuf *m_freem(struct mbuf *m)
{
	struct mbuf *n;
	do {
		n = m->m_next;
		m_free(m);
	} while ((m = n) != NULL);
	return NULL;
}

struct mbuf *
m_clget(struct mbuf *m, int how, struct ifnet *ifp, u32 pktlen)
{
	struct mbuf *m0 = NULL;

	if (m == NULL){
		m0 = m_gethdr(0, MT_DATA);
		if (!m0)
			return NULL;

		m = m0;
	}

	m->m_ext.ext_buf = kzalloc(MCLBYTES);
	if (!m->m_ext.ext_buf){
		m_free(m0);
		return NULL;
	}
	mcl_allocated++;
	m->m_data = m->m_ext.ext_buf;
	m->m_flags |= M_EXT ;
	m->m_ext.ext_refcnt++;
	m->m_ext.ext_free = NULL;
	return m;
}

struct mbuf *
m_getclr(int how, short type)
{

	struct mbuf *m;
	MGET(m, how, type);
	if (m == NULL)
		return (NULL);
	memset((void *)(mtod(m, caddr_t)), 0, MLEN);
	return (m);

}

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(struct mbuf *m, int off, int len, caddr_t cp)
{
	unsigned count;

	if (off < 0 || len < 0)
		LOG_ERROR("m_copydata");
	while (off > 0) {
		if (m == 0)
			LOG_ERROR("m_copydata");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	LOG_INFO("len:%d", len);

	if(m->m_flags & M_EXT) {
		count = min(len, MCLBYTES);
		memcpy((void *)cp, (void *)mtod(m, caddr_t), count);
		return ;
	}

	while (len > 0) {
		if (m == NULL)
			LOG_ERROR("m == NULL, len:%d", len);
		count = min(m->m_len - off, len);
		memcpy((void *)cp, (void *)(mtod(m, caddr_t) + off), count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

/*
 * Rearrange an mbuf chain so that len bytes are contiguous
 * and in the data area of an mbuf (so that mtod will work
 * for a structure of size len).  Returns the resulting
 * mbuf chain on success, frees it and returns null on failure.
 */
struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;

	/*
	 * If first mbuf has no cluster, and has room for len bytes
	 * without shifting current data, pullup into it,
	 * otherwise allocate a new mbuf to prepend to the chain.
	 */
	if ((n->m_flags & M_EXT) == 0 && n->m_next &&
	    n->m_data + len < &n->m_dat[MLEN]) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else if ((n->m_flags & M_EXT) != 0 && len > MHLEN && n->m_next &&
	    n->m_data + len < &n->m_ext.ext_buf[MCLBYTES]) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MAXMCLBYTES)
			goto bad;
		MGET(m, M_DONTWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		if (len > MHLEN) {
			MCLGETI(m, M_DONTWAIT, NULL, len);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				goto bad;
			}
		}
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(m, n);
	}

	do {
		count = min(len, n->m_len);
		memcpy(mtod(m, caddr_t) + m->m_len, mtod(n, caddr_t),
		    (unsigned)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_freem(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void)m_freem(m);
		goto bad;
	}
	m->m_next = n;

	return (m);
bad:
	if (n)
		m_freem(n);
	return (NULL);
}


int
m_leadingspace(struct mbuf *m)
{
	return (m->m_flags & M_EXT ? m->m_data - m->m_ext.ext_buf :
	    m->m_flags & M_PKTHDR ? m->m_data - m->m_pktdat :
	    m->m_data - m->m_dat);
}

/*
 * Ensure len bytes of contiguous space at the beginning of the mbuf chain
 */
struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (len > MHLEN)
		LOG_ERROR("mbuf prepend length too big");

	if (M_LEADINGSPACE(m) >= len) {
		m->m_data -= len;
		m->m_len += len;
	} else {
		MGET(mn, how, m->m_type);
		if (mn == NULL) {
			m_freem(m);
			return (NULL);
		}
		if (m->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(mn, m);
		mn->m_next = m;
		m = mn;
		MH_ALIGN(m, len);
		m->m_len = len;
	}
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len += len;
	return (m);
}


/*
 * Duplicate mbuf pkthdr from from to to.
 * from must have M_PKTHDR set, and to must be empty.
 */
int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int wait)
{

	assert(from->m_flags & M_PKTHDR);

	to->m_flags = (to->m_flags & (M_EXT | M_CLUSTER));
	to->m_flags |= (from->m_flags & M_COPYFLAGS);
	to->m_pkthdr = from->m_pkthdr;

	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;

	return 0;
}




struct mbuf *
_m_copym(struct mbuf *m0, int offset, int len, int wait, int deep)
{
	struct mbuf *m, *n, **np, *top;
	int copyhdr = 0;

	if (offset < 0 || len < 0)
		return NULL;


	if (offset == 0 && (m0->m_flags & M_PKTHDR))
		copyhdr = 1;

	if ((m = m_getptr(m0, offset, &offset)) == NULL)
		return NULL;


	np = &top;
	top = NULL;

	while (len > 0){
		if (m == NULL){
			if (len != M_COPYALL)
				LOG_ERROR("m==NULL");
			break;
		}
		MGET(n, wait, m->m_type);
		*np = n;
		if (n == NULL)
			goto END;
		if (copyhdr){
			if (m_dup_pkthdr(n, m, wait) < 0)
				goto END;
			if (len != M_COPYALL)
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}

		n->m_len = min(len, m->m_len - offset);
		if (m->m_flags & M_EXT){
			if (!deep){
				n->m_data = m->m_data;
				n->m_ext = m->m_ext;
				m->m_extrefcnt++;
				n->m_flags |= (m->m_flags & (M_EXT | M_CLUSTER));
			} else {
				//deep copy
				MCLGET(n, wait);
				n->m_len = min(MCLBYTES,len);
				n->m_len = min(n->m_len, m->m_len - offset);
				memcpy((void *)mtod(n, caddr_t), (void *)(mtod(m, caddr_t)+offset), n->m_len);
			}
		} else
			memcpy((void *)mtod(n, caddr_t), (void *)(mtod(m, caddr_t)+offset), n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;


		m = m->m_next;
		offset = 0;
		np = &n->m_next;
	}
	return top;
END:
	m_freem(top);
	return NULL;
}


struct mbuf*
m_copym(struct mbuf *m, int off, int len, int wait)
{
	//shallow copy
	return _m_copym(m, off, len, wait, 0);
}

/*
 * Return a pointer to mbuf/offset of location in mbuf chain.
 */
struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off)
{
	while (loc >= 0) {
		/* Normal end of search */
		if (m->m_len > loc) {
	    		*off = loc;
	    		return (m);
		} else {
	    		loc -= m->m_len;

	    		if (m->m_next == NULL) {
				if (loc == 0) {
 					/* Point at the end of valid data */
		    			*off = m->m_len;
		    			return (m);
				} else {
		  			return (NULL);
				}
	    		} else {
				m = m->m_next;
			}
		}
    	}

	return (NULL);
}


/*
 * Concatenate mbuf chain n to m.
 * Both chains must be of the same type (e.g. MT_DATA).
 * Any m_pkthdr is not updated.
 */
void
m_cat(struct mbuf *m,struct mbuf *n)
{
	while (m->m_next)
		m = m->m_next;
	while (n) {
		if ((m->m_flags & M_EXT) ||
		    m->m_data + m->m_len + n->m_len >= &m->m_dat[MLEN]) {
			/* just join the two chains */
			m->m_next = n;
			return;
		}
		/* splat the data from one into the other */
		memcpy((void *)(mtod(m, caddr_t) + m->m_len), (void *)mtod(n, caddr_t),
		    (size_t)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

void
m_adj(struct mbuf *mp, int req_len)
{
	int len = req_len;
	struct mbuf *m;
	int count;

	if ((m = mp) == NULL)
		return;
	if (len >= 0) {
		/*
		 * Trim from head.
		 */
		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		m = mp;
		if (mp->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= (req_len - len);
	} else {
		/*
		 * Trim from tail.  Scan the mbuf chain,
		 * calculating its length and finding the last mbuf.
		 * If the adjustment only affects this mbuf, then just
		 * adjust and return.  Otherwise, rescan and truncate
		 * after the remaining size.
		 */
		len = -len;
		count = 0;
		for (;;) {
			count += m->m_len;
			if (m->m_next == (struct mbuf *)0)
				break;
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if (mp->m_flags & M_PKTHDR)
				mp->m_pkthdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;
		/*
		 * Correct length for chain is "count".
		 * Find the mbuf with last data, adjust its length,
		 * and toss data from remaining mbufs on chain.
		 */
		m = mp;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len = count;
		for (; m; m = m->m_next) {
			if (m->m_len >= count) {
				m->m_len = count;
				break;
			}
			count -= m->m_len;
		}
		while ((m = m->m_next) != NULL)
			m->m_len = 0;
	}
}

/*
 * Routine to copy from device local memory into mbufs.
 * Note that `off' argument is offset into first mbuf of target chain from
 * which to begin copying the data to.
 */
struct mbuf *
m_devget(char *buf, int totlen, int off, struct ifnet *ifp,
    void (*copy)(char *from, caddr_t to, u32 len))
{
	struct mbuf *m;
	struct mbuf *top = NULL, **mp = &top;
	int len;
	char *cp;
	char *epkt;

	cp = buf;
	epkt = cp+totlen;

	if (off < 0 || off > MHLEN)
		return (NULL);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (!m)
		return NULL;

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;

	len = MHLEN;

	while (totlen > 0){
		if (top != NULL){
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL){
				m_freem(top);
				return NULL;
			}
			len = MLEN;
		}

		if (totlen + off >= MINCLSIZE){
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;

		} else {
			if (top == NULL && totlen + off + max_linkhdr <= len){
				m->m_data += max_linkhdr;
				len -= max_linkhdr;
			}

		}

		if (off){
			m->m_data += off;
			len -= off;
			off = 0;
		}

		m->m_len = len = min(totlen, len);

		if (copy)
			copy(buf, mtod(m, caddr_t), (size_t)len);
		else
			memcpy(mtod(m, caddr_t), buf, (size_t)len);

		buf += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
	}

	return top;

}

