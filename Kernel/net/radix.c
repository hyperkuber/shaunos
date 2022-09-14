/*
 * radix.c
 *
 *  Created on: Aug 2, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/net/radix.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/sys/domain.h>
#include <kernel/assert.h>
#include <kernel/malloc.h>
#include <kernel/net/route.h>
int max_keylen;
char *rn_zeros;
char *rn_ones;
char *maskedkey;

//struct	radix_node_head *rt_tables[AF_MAX+1];
struct radix_node_head *mask_rnhead;
struct radix_mask *rn_mkfreelist;
static char normal_chars[] = {0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, -1};
static char *addmask_key;

#define rn_masktop (mask_rnhead->rnh_treetop)

#define MKGet(m) {\
	if (rn_mkfreelist) {\
		m = rn_mkfreelist; \
		rn_mkfreelist = (m)->rm_mklist; \
	} else \
		m = kmalloc(sizeof(*(m))); }


#define MKFree(m) { (m)->rm_mklist = rn_mkfreelist; rn_mkfreelist = (m);}





struct radix_node *
rn_newpair(void *v, int b, struct radix_node nodes[])
{
	struct radix_node *tt = nodes, *t = tt+1;
	t->rn_b = b;
	t->rn_bmask = 0x80 >> (b & 7);
	t->rn_l = tt;
	t->rn_off = b >> 3;
	tt->rn_b = -1;
	tt->rn_key = (caddr_t)v;
	tt->rn_p = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;
	return t;
}


struct radix_node *
rn_search(void *v_arg, struct radix_node *head)
{
	struct radix_node *x;
	caddr_t v;
	for (x=head, v=v_arg; x->rn_b >= 0;){
		if (x->rn_bmask & v[x->rn_off])
			x = x->rn_r;
		else
			x = x->rn_l;
	}

	return x;
}


struct radix_node *
rn_match(void *v_arg, struct radix_node_head *head)
{
	caddr_t v = v_arg;
	caddr_t cp = v, cp2, cp3;
	caddr_t cplim, mstart;
	struct radix_node *t = head->rnh_treetop, *x;
	struct radix_node *saved_t, *top = t;
	int off = t->rn_off, vlen = *(u8 *)cp, matched_off;
	LOG_DEBG("in");
	for (; t->rn_b >= 0; ){
		if (t->rn_bmask & cp[t->rn_off])
			t = t->rn_r;
		else
			t = t->rn_l;
	}

	cp += off;
	cp2 = t->rn_key + off;
	cplim = v+vlen;

	for (; cp < cplim; cp++, cp2++)
		if (*cp != *cp2)
			goto on1;


	if ((t->rn_flags & RNF_ROOT) && t->rn_dupedkey)
		t = t->rn_dupedkey;
	LOG_DEBG("out:%x", t);
	return t;

on1:
	/*
	 * i really can't understand it
	 * say, cp points to 140.252.13.60(8c.fc.0d.3c)
	 * and rn_key = 140.252.13.32(8c.fc.0d.20),
	 * so matched offset = 7(starts from &sockaddr_in),
	 * and if rn_mask(subnet mask) is 255.255.255.254(ff.ff.ff.e0),
	 * so
	 * 3c(*cp)  = 0011 1100
	 * 20(*cp2) = 0010 0000
	 * 3c ^ 20  = 0001 1100
	 * e0(*cp3) = 1110 0000
	 * 		   &= 0000 0000
	 * if result is 0, ok, we matched successfully,
	 * and another way is , as we all know,
	 * if dest ip & submask == key, ok, we got it,
	 * say, 3c & e0 == 20, this is the simplest way,
	 * and who can tell me why the old code use
	 * this shit way below
	 *
	 * btw, the idea is steven's, not mine. he~
	 */
	matched_off = cp - v;
	saved_t = t;
	do {
		if (t->rn_mask){
			cp3 = matched_off + t->rn_mask;
			cp2 = matched_off + t->rn_key;
			for (; cp < cplim; cp++){
				if ((*cp2++ ^ *cp) & *cp3++)
					break;
			}

			if (cp == cplim)
				return t;
			cp = matched_off + v;
		}
	} while ((t = t->rn_dupedkey) != NULL);

	t = saved_t;

	do {

		struct radix_mask *m;
		t = t->rn_p;
		if ((m = t->rn_mklist) != NULL){
			off = min(t->rn_off, matched_off);
			mstart = maskedkey + off;
			do {
				cp2 = mstart;
				cp3 = m->rm_mask + off;
				for (cp = v+off; cp < cplim;){
					*cp2++ = *cp++ & *cp3++;
				}

				x = rn_search(maskedkey, t);
				while (x && x->rn_mask != m->rm_mask)
					x = x->rn_dupedkey;

				if (x && memcmp((void *)mstart, x->rn_key+off, vlen - off) == 0)
					return x;

			} while ((m = m->rm_mklist) != NULL);
		}
	} while ( t != top);
	LOG_DEBG("end,t=%x", 0);
	return NULL;
}


int
rn_refines(void *m_arg, void *n_arg)
{
	caddr_t m = m_arg, n = n_arg;
	caddr_t lim, lim2 = lim = n + *(u8 *)n;
	int longer = (*(u8 *)n++) - (int)(*(u8 *)m++);
	int masks_are_equal = 1;
	LOG_DEBG("in");
	if (longer > 0)
		lim -= longer;
	while (n < lim) {
		if (*n & ~(*m))
			return 0;
		if (*n++ != *m++)
			masks_are_equal = 0;
	}
	while (n < lim2)
		if (*n++)
			return 0;
	if (masks_are_equal && (longer < 0))
		for (lim2 = m - longer; m < lim2; )
			if (*m++)
				return 1;

	LOG_DEBG("end, ret:%d", !masks_are_equal);
	return (!masks_are_equal);
}

struct radix_node *
rn_insert(void *v_arg, struct radix_node_head *head,
		int *dupentry, struct radix_node nodes[])
{
	caddr_t v = v_arg;
	struct radix_node *top = head->rnh_treetop;
	int head_off = top->rn_off, vlen = (int)*((u8 *)v);
	struct radix_node *t = rn_search(v_arg, top);
	caddr_t cp = v + head_off;
	int b;
	struct radix_node *tt;
    /*
	 * Find first bit at which v and t->rn_key differ
	 */
    {
	caddr_t cp2 = t->rn_key + head_off;
	int cmp_res;
	caddr_t cplim = v + vlen;
	while (cp < cplim)
		if (*cp2++ != *cp++)
			goto on1;
	*dupentry = 1;
	return t;
on1:
	*dupentry = 0;
	cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
	for (b = (cp - v) << 3; cmp_res; b--)
		cmp_res >>= 1;
    }
    {
    	struct radix_node *p, *x = top;
    	cp = v;
    	do {
			p = x;
			if (cp[x->rn_off] & x->rn_bmask)
				x = x->rn_r;
			else x = x->rn_l;
    	} while (b > (unsigned) x->rn_b); /* x->rn_b < b && x->rn_b >= 0 */

		t = rn_newpair(v_arg, b, nodes); tt = t->rn_l;
		if ((cp[p->rn_off] & p->rn_bmask) == 0)
			p->rn_l = t;
		else
			p->rn_r = t;
		x->rn_p = t; t->rn_p = p; /* frees x, p as temp vars below */
		if ((cp[t->rn_off] & t->rn_bmask) == 0) {
			t->rn_r = x;
		} else {
			t->rn_r = tt; t->rn_l = x;
		}
    }
	return (tt);
}


struct radix_node *
rn_addmask(void *n_arg, int search, int skip)
{
	caddr_t netmask = (caddr_t)n_arg;
	struct radix_node *x;
	caddr_t cp, cplim;
	int b = 0, mlen, j;
	int maskduplicated, m0, isnormal;
	struct radix_node *saved_x;
	static int last_zeroed = 0;

	if ((mlen = *(u8 *)netmask) > max_keylen)
		mlen = max_keylen;
	if (skip == 0)
		skip = 1;
	if (mlen <= skip)
		return (mask_rnhead->rnh_nodes);
	if (skip > 1)
		Bcopy(rn_ones + 1, addmask_key + 1, skip - 1);
	if ((m0 = mlen) > skip)
		Bcopy(netmask + skip, addmask_key + skip, mlen - skip);
	/*
	 * Trim trailing zeroes.
	 */
	for (cp = addmask_key + mlen; (cp > addmask_key) && cp[-1] == 0;)
		cp--;
	mlen = cp - addmask_key;
	if (mlen <= skip) {
		if (m0 >= last_zeroed)
			last_zeroed = mlen;
		return (mask_rnhead->rnh_nodes);
	}
	if (m0 < last_zeroed)
		Bzero(addmask_key + m0, last_zeroed - m0);
	*addmask_key = last_zeroed = mlen;
	x = rn_search(addmask_key, rn_masktop);
	if (Bcmp(addmask_key, x->rn_key, mlen) != 0)
		x = 0;
	if (x || search)
		return (x);
	R_Malloc(x, struct radix_node *, max_keylen + 2 * sizeof (*x));
	if ((saved_x = x) == NULL)
		return (0);
	Bzero(x, max_keylen + 2 * sizeof (*x));
	netmask = cp = (caddr_t)(x + 2);
	Bcopy(addmask_key, cp, mlen);
	x = rn_insert(cp, mask_rnhead, &maskduplicated, x);
	if (maskduplicated) {
		LOG_ERROR("rn_addmask: mask impossibly already in tree");
		Free(saved_x);
		return (x);
	}
	/*
	 * Calculate index of mask, and check for normalcy.
	 */
	cplim = netmask + mlen; isnormal = 1;
	for (cp = netmask + skip; (cp < cplim) && *(u8 *)cp == 0xff;)
		cp++;
	if (cp != cplim) {
		for (j = 0x80; (j & *cp) != 0; j >>= 1)
			b++;
		if (*cp != normal_chars[b] || cp != (cplim - 1))
			isnormal = 0;
	}
	b += (cp - netmask) << 3;
	x->rn_b = -1 - b;
	if (isnormal)
		x->rn_flags |= RNF_NORMAL;
	return (x);
}

static int	/* XXX: arbitrary ordering for non-contiguous masks */
rn_lexobetter(void *m_arg, void *n_arg)
{
	u8 *mp = m_arg, *np = n_arg, *lim;

	if (*mp > *np)
		return 1;  /* not really, but need to check longer one first */
	if (*mp == *np)
		for (lim = mp + *mp; mp < lim;)
			if (*mp++ > *np++)
				return 1;
	return 0;
}

static struct radix_mask *
rn_new_radix_mask(struct radix_node *tt, struct radix_mask *next)
{
	struct radix_mask *m;
	MKGet(m);
	if (m == NULL) {
		LOG_ERROR("Mask for route not entered\n");
		return (NULL);
	}
	memset((void *)m, 0, sizeof *m);
	m->rm_b = tt->rn_b;
	m->rm_flags = tt->rn_flags;
	if (tt->rn_flags & RNF_NORMAL)
		m->rm_leaf = tt;
	else
		m->rm_mask = tt->rn_mask;
	m->rm_mklist = next;
	tt->rn_mklist = m;
	return m;
}



struct radix_node *
rn_addroute(void *v_arg, void *n_arg,
		struct radix_node_head *head,
		struct radix_node treenodes[])
{
	caddr_t v = (caddr_t)v_arg, netmask = (caddr_t)n_arg;
	register struct radix_node *t, *x = 0, *tt;
	struct radix_node *saved_tt, *top = head->rnh_treetop;
	short b = 0, b_leaf = 0;
	int keyduplicated;
	caddr_t mmask;
	struct radix_mask *m, **mp;

	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask)  {
		if ((x = rn_addmask(netmask, 0, top->rn_off)) == NULL)
			return (0);
		b_leaf = x->rn_b;
		b = -1 - x->rn_b;
		netmask = x->rn_key;
	}
	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	saved_tt = tt = rn_insert(v, head, &keyduplicated, treenodes);
	if (keyduplicated) {
		for (t = tt; tt; t = tt, tt = tt->rn_dupedkey) {
			if (tt->rn_mask == netmask)
				return (0);
			if (netmask == 0 ||
			    (tt->rn_mask &&
			     ((b_leaf < tt->rn_b) || /* index(netmask) > node */
			       rn_refines(netmask, tt->rn_mask) ||
			       rn_lexobetter(netmask, tt->rn_mask))))
				break;
		}
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * We sort the masks for a duplicated key the same way as
		 * in a masklist -- most specific to least specific.
		 * This may require the unfortunate nuisance of relocating
		 * the head of the list.
		 *
		 * We also reverse, or doubly link the list through the
		 * parent pointer.
		 */
		if (tt == saved_tt) {
			struct	radix_node *xx = x;
			/* link in at head of list */
			(tt = treenodes)->rn_dupedkey = t;
			tt->rn_flags = t->rn_flags;
			tt->rn_p = x = t->rn_p;
			t->rn_p = tt;
			if (x->rn_l == t) x->rn_l = tt; else x->rn_r = tt;
			saved_tt = tt; x = xx;
		} else {
			(tt = treenodes)->rn_dupedkey = t->rn_dupedkey;
			t->rn_dupedkey = tt;
			tt->rn_p = t;
			if (tt->rn_dupedkey)
				tt->rn_dupedkey->rn_p = tt;
		}

		tt->rn_key = (caddr_t) v;
		tt->rn_b = -1;
		tt->rn_flags = RNF_ACTIVE;
	}
	/*
	 * Put mask in tree.
	 */
	if (netmask) {
		tt->rn_mask = netmask;
		tt->rn_b = x->rn_b;
		tt->rn_flags |= x->rn_flags & RNF_NORMAL;
	}
	t = saved_tt->rn_p;
	if (keyduplicated)
		goto on2;
	b_leaf = -1 - t->rn_b;
	if (t->rn_r == saved_tt) x = t->rn_l; else x = t->rn_r;
	/* Promote general routes from below */
	if (x->rn_b < 0) {
	    for (mp = &t->rn_mklist; x; x = x->rn_dupedkey)
		if (x->rn_mask && (x->rn_b >= b_leaf) && x->rn_mklist == 0) {
			*mp = m = rn_new_radix_mask(x, 0);
			if (m)
				mp = &m->rm_mklist;
		}
	} else if (x->rn_mklist) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
			if (m->rm_b >= b_leaf)
				break;
		t->rn_mklist = m; *mp = 0;
	}
on2:
	/* Add new route to highest possible ancestor's list */
	if ((netmask == 0) || (b > t->rn_b ))
		return tt; /* can't lift at all */
	b_leaf = tt->rn_b;
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * Need same criteria as when sorting dupedkeys to avoid
	 * double loop on deletion.
	 */
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
		if (m->rm_b < b_leaf)
			continue;
		if (m->rm_b > b_leaf)
			break;
		if (m->rm_flags & RNF_NORMAL) {
			mmask = m->rm_leaf->rn_mask;
			if (tt->rn_flags & RNF_NORMAL) {
				LOG_ERROR("Non-unique normal route, mask not entered");
				return tt;
			}
		} else
			mmask = m->rm_mask;
		if (mmask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		if (rn_refines(netmask, mmask) || rn_lexobetter(netmask, mmask))
			break;
	}
	*mp = rn_new_radix_mask(tt, *mp);
	return tt;
}


struct radix_node *
rn_lookup(void *v_arg, void *m_arg, struct radix_node_head *head)
{
	struct radix_node *x;
	caddr_t netmask = 0;

	if (m_arg) {
		if ((x = rn_addmask(m_arg, 1, head->rnh_treetop->rn_off)) == NULL)
			return (0);
		netmask = x->rn_key;
	}
	x = rn_match(v_arg, head);
	if (x && netmask) {
		while (x && x->rn_mask != netmask)
			x = x->rn_dupedkey;
	}
	return x;
}

struct radix_node *
rn_delete(void *v_arg, void *netmask_arg, struct radix_node_head *head)
{
	struct radix_node *t, *p, *x, *tt;
	struct radix_mask *m, *saved_m, **mp;
	struct radix_node *dupedkey, *saved_tt, *top;
	caddr_t v, netmask;
	int b, head_off, vlen;

	v = v_arg;
	netmask = netmask_arg;
	x = head->rnh_treetop;
	tt = rn_search(v, x);
	head_off = x->rn_off;
	vlen =  *(u8 *)v;
	saved_tt = tt;
	top = x;
	if (tt == 0 ||
	    Bcmp(v + head_off, tt->rn_key + head_off, vlen - head_off))
		return NULL;
	/*
	 * Delete our route from mask lists.
	 */
	if (netmask) {
		if ((x = rn_addmask(netmask, 1, head_off)) == NULL)
			return (0);
		netmask = x->rn_key;
		while (tt->rn_mask != netmask)
			if ((tt = tt->rn_dupedkey) == 0)
				return (0);
	}
	if (tt->rn_mask == 0 || (saved_m = m = tt->rn_mklist) == NULL)
		goto on1;
	if (tt->rn_flags & RNF_NORMAL) {
		if (m->rm_leaf != tt || m->rm_refs > 0) {
			LOG_ERROR("rn_delete: inconsistent annotation");
			return 0;  /* dangling ref could cause disaster */
		}
	} else {
		if (m->rm_mask != tt->rn_mask) {
			LOG_ERROR("rn_delete: inconsistent annotation");
			goto on1;
		}
		if (--m->rm_refs >= 0)
			goto on1;
	}
	b = -1 - tt->rn_b;
	t = saved_tt->rn_p;
	if (b > t->rn_b)
		goto on1; /* Wasn't lifted at all */
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
		if (m == saved_m) {
			*mp = m->rm_mklist;
			MKFree(m);
			break;
		}
	if (m == 0) {
		LOG_ERROR("rn_delete: couldn't find our annotation");
		if (tt->rn_flags & RNF_NORMAL)
			return NULL; /* Dangling ref to us */
	}
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT)
		return NULL;
	t = tt->rn_p;
	dupedkey = saved_tt->rn_dupedkey;
	if (dupedkey) {
		/*
		 * Here, tt is the deletion target, and
		 * saved_tt is the head of the dupedkey chain.
		 */
		if (tt == saved_tt) {
			x = dupedkey; x->rn_p = t;
			if (t->rn_l == tt) t->rn_l = x; else t->rn_r = x;
		} else {
			/* find node in front of tt on the chain */
			for (x = p = saved_tt; p && p->rn_dupedkey != tt;)
				p = p->rn_dupedkey;
			if (p) {
				p->rn_dupedkey = tt->rn_dupedkey;
				if (tt->rn_dupedkey)
					tt->rn_dupedkey->rn_p = p;
			} else LOG_ERROR("rn_delete: couldn't find us");
		}
		t = tt + 1;
		if  (t->rn_flags & RNF_ACTIVE) {
#ifndef RN_DEBUG
			*++x = *t; p = t->rn_p;
#else
			b = t->rn_info; *++x = *t; t->rn_info = b; p = t->rn_p;
#endif
			if (p->rn_l == t) p->rn_l = x; else p->rn_r = x;
			x->rn_l->rn_p = x; x->rn_r->rn_p = x;
		}
		goto out;
	}
	if (t->rn_l == tt) x = t->rn_r; else x = t->rn_l;
	p = t->rn_p;
	if (p->rn_r == t) p->rn_r = x; else p->rn_l = x;
	x->rn_p = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist) {
		if (x->rn_b >= 0) {
			for (mp = &x->rn_mklist; (m = *mp);)
				mp = &m->rm_mklist;
			*mp = t->rn_mklist;
		} else {
			/* If there are any key,mask pairs in a sibling
			   duped-key chain, some subset will appear sorted
			   in the same order attached to our mklist */
			for (m = t->rn_mklist; m && x; x = x->rn_dupedkey)
				if (m == x->rn_mklist) {
					struct radix_mask *mm = m->rm_mklist;
					x->rn_mklist = 0;
					if (--(m->rm_refs) < 0)
						MKFree(m);
					m = mm;
				}
			if (m)
				LOG_ERROR("%s %x at %x\n",
					    "rn_delete: Orphaned Mask", m, x);
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
#ifndef RN_DEBUG
		*t = *x;
#else
		b = t->rn_info; *t = *x; t->rn_info = b;
#endif
		t->rn_l->rn_p = t; t->rn_r->rn_p = t;
		p = x->rn_p;
		if (p->rn_l == x) p->rn_l = t; else p->rn_r = t;
	}
out:
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return (tt);
}

int
rn_walktree(struct radix_node_head *h, int (*f)(), void *w)
{
	int error;
	struct radix_node *base, *next;
	struct radix_node *rn = h->rnh_treetop;
	/*
	 * This gets complicated because we may delete the node
	 * while applying the function f to it, so we need to calculate
	 * the successor node in advance.
	 */
	/* First time through node, go left */
	while (rn->rn_b >= 0)
		rn = rn->rn_l;
	for (;;) {
		base = rn;
		/* If at right child go back up, otherwise, go right */
		while (rn->rn_p->rn_r == rn && (rn->rn_flags & RNF_ROOT) == 0)
			rn = rn->rn_p;
		/* Find the next *leaf* since next node might vanish, too */
		for (rn = rn->rn_p->rn_r; rn->rn_b >= 0;)
			rn = rn->rn_l;
		next = rn;
		/* Process leaves */
		while ((rn = base) != NULL) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT) && (error = (*f)(rn, w)))
				return (error);
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT)
			return (0);
	}
	/* NOTREACHED */
}


int rn_inithead(void **head, int off)
{
	struct radix_node_head *rnh;
	struct radix_node *top, *left, *right;
	if (*head)
		return 1;

	LOG_DEBG("in");
	rnh = (struct radix_node_head *)kzalloc(sizeof(*rnh));
	if (rnh == NULL)
		return 0;
	*head = rnh;

	top = rn_newpair(rn_zeros, off, rnh->rnh_nodes);
	right = rnh->rnh_nodes + 2;
	top->rn_r = right;
	top->rn_p = top;
	left = top->rn_l;
	left->rn_flags = top->rn_flags = RNF_ROOT | RNF_ACTIVE;
	left->rn_b = -1 -off;
	*right = *left;
	right->rn_key = rn_ones;


	rnh->rnh_addaddr = rn_addroute;
	rnh->rnh_deladdr = rn_delete;
	rnh->rnh_matchaddr = rn_match;
	rnh->rnh_lookup	= rn_lookup;
	rnh->rnh_walktree = rn_walktree;
	rnh->rnh_treetop = top;
	LOG_DEBG("end");
	return 1;
}



void rn_init()
{

	char *cp, *cplim;
	struct domain *dom;

	LOG_DEBG("in");
	for (dom = domains; dom; dom=dom->dom_next)
		if (dom->dom_maxrtkey > max_keylen)
			max_keylen = dom->dom_maxrtkey;
	if (max_keylen == 0)
		LOG_ERROR("max_keylen == 0");
	rn_zeros = (char *)kzalloc(max_keylen*3);
	assert(rn_zeros != NULL);

	rn_ones = cp = rn_zeros + max_keylen;
	maskedkey = cplim = rn_ones + max_keylen;
	while (cp < cplim)
		*cp++ = 0xFF;


	if (rn_inithead((void **)&mask_rnhead, 0) == 0)
		LOG_ERROR("rn init error");
	LOG_DEBG("end");
}










