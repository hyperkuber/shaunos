/*
 * radix.h
 *
 *  Created on: Aug 2, 2013
 *      Author: root
 */

#ifndef SHAUN_RADIX_H_
#define SHAUN_RADIX_H_

#include <kernel/types.h>


/* struct radix_mask {
	short	rm_b;
	char rm_unused;
	unsigned char rm_flags;
	struct radix_mask *rm_mklist;
	caddr_t rm_mask;
	int rm_refs;
} ;
*/
extern struct radix_mask {
	short	rm_b;			/* bit offset; -1-index(netmask) */
	char	rm_unused;		/* cf. rn_bmask */
	u8		rm_flags;		/* cf. rn_flags */
	struct	radix_mask *rm_mklist;	/* more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}rm_rmu;
	int	rm_refs;		/* # of references to this struct */
} *rn_mkfreelist;

#define rm_mask rm_rmu.rmu_mask
#define rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */



struct radix_node {
	struct radix_mask *rn_mklist;
	struct radix_node *rn_p;
	short	rn_b;
	char rn_bmask;
	unsigned char rn_flags;
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {
			caddr_t rn_key;
			caddr_t rn_mask;
			struct radix_node *rn_dupedkey;
		} rn_leaf;

		struct {
			int rn_off;
			struct radix_node *rn_l;
			struct radix_node *rn_r;
		}rn_node;
	}rn_u;
};


#define rn_dupedkey	rn_u.rn_leaf.rn_dupedkey
#define rn_key	rn_u.rn_leaf.rn_key
#define rn_mask rn_u.rn_leaf.rn_mask
#define rn_off	rn_u.rn_node.rn_off
#define rn_l	rn_u.rn_node.rn_l
#define rn_r	rn_u.rn_node.rn_r



struct radix_node_head {
	struct radix_node *rnh_treetop;
	int rnh_addrsize;
	int rnh_pktsize;
	struct radix_node *(*rnh_addaddr)
			(void *v, void *mask,
					struct radix_node_head *head, struct radix_node nodes[]);

	struct radix_node *(*rnh_addpkt)
			(void *v, void *mask,
					struct radix_node_head *head, struct radix_node nodes[]);
	struct radix_node *(*rnh_deladdr)
			(void *v, void *mask, struct radix_node_head *head);
	struct radix_node *(*rnh_delpkt)
			(void *v, void *mask, struct radix_node_head *head);
	struct radix_node *(*rnh_matchaddr)
			(void *v, struct radix_node_head *head);
	struct radix_node *(*rnh_lookup)
			(void *v, void *mask, struct radix_node_head *head);
	struct radix_node *(*rnh_matchpkt)
			(void *v, struct radix_node_head *head);

	int (*rnh_walktree)(struct radix_node_head *head, int (*f)(), void *w);

	struct radix_node rnh_nodes[3];
};







extern void rn_init();
extern struct radix_node *
rn_addmask(void *n_arg, int search, int skip);
extern int
rn_inithead(void **head, int off);

extern int
rn_refines(void *m_arg, void *n_arg);
#endif /* SHAUN_RADIX_H_ */
