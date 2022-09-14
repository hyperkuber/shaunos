/*
 * in_val.h
 *
 *  Created on: Jun 28, 2013
 *      Author: root
 */

#ifndef SHAUN_IN_VAL_H_
#define SHAUN_IN_VAL_H_

#include <kernel/types.h>
#include <kernel/net/if.h>
#include <kernel/sys/netinet/in.h>
#include <list.h>

struct in_ifaddr;

struct in_multi {
	struct	in_addr inm_addr;	/* IP multicast address */
	struct	in_ifaddr *inm_ia;	/* back pointer to in_ifaddr */
	struct	ifnet *inm_ifp;		/* back pointer to ifnet */
	u32	inm_refcount;		/* no. membership claims by sockets */
	u32	inm_timer;		/* IGMP membership report timer */
	struct {
		struct in_multi *next;
		struct in_multi **prev;
	} inm_list;	/* list of multicast addresses */
	u32	inm_state;		/* state of membership */
	struct	router_info *inm_rti;	/* router version info */
};
#define inm_next inm_list.next
#define inm_prev inm_list.prev


struct in_ifaddr {
	struct ifaddr	ia_ifa;
#define ia_ifp	ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
	struct in_ifaddr	*ia_next;
	u32	ia_net;
	u32	ia_netmask;
	u32	ia_subnet;
	u32	ia_subnetmask;
	struct in_addr	ia_netbroadcast;
	struct sockaddr_in	ia_addr;
	struct sockaddr_in	ia_dstaddr;
#define ia_broadaddr	ia_dstaddr
	struct sockaddr_in	ia_sockmask;
	struct in_multi	*ia_multiaddrs;

};

struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in ifra_addr;
	struct	sockaddr_in ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
	struct	sockaddr_in ifra_mask;
};

/*
 * Macro for finding the internet address structure (in_ifaddr) corresponding
 * to a given interface (ifnet structure).
 */
#define IFP_TO_IA(ifp, ia)						\
	/* struct ifnet *ifp; */					\
	/* struct in_ifaddr *ia; */					\
		do {									\
			for ((ia) = in_ifaddr;				\
			(ia) != NULL && (ia)->ia_ifp != (ifp);	\
			(ia) = (ia)->ia_next)				\
			continue;						\
		} while (0)

struct in_multistep {
	struct in_ifaddr *i_ia;
	struct in_multi *i_inm;
};



#define IN_LOOKUP_MULTI(addr, ifp, inm)	\
	do {	\
		struct in_ifaddr *ia;			\
		IFP_TO_IA((ifp), ia);			\
		if (ia == NULL)					\
			(inm) = NULL;				\
		else							\
			for ((inm) = ia->ia_multiaddrs;		\
			(inm) != NULL	&&					\
			(inm)->inm_addr.s_addr != (addr).s_addr;	\
			(inm) = (inm)->inm_list.next)			\
				continue;							\
	} while (0)



/*
 * Macro to step through all of the in_multi records, one at a time.
 * The current position is remembered in "step", which the caller must
 * provide.  IN_FIRST_MULTI(), below, must be called to initialize "step"
 * and get the first record.  Both macros return a NULL "inm" when there
 * are no remaining records.
 */
#define IN_NEXT_MULTI(step, inm) \
	/* struct in_multistep  step; */ \
	/* struct in_multi *inm; */ \
{ \
	if (((inm) = (step).i_inm) != NULL) \
		(step).i_inm = (inm)->inm_next; \
	else \
		while ((step).i_ia != NULL) { \
			(inm) = (step).i_ia->ia_multiaddrs; \
			(step).i_ia = (step).i_ia->ia_next; \
			if ((inm) != NULL) { \
				(step).i_inm = (inm)->inm_next; \
				break; \
			} \
		} \
}

#define IN_FIRST_MULTI(step, inm) \
	/* struct in_multistep step; */ \
	/* struct in_multi *inm; */ \
{ \
	(step).i_ia = in_ifaddr; \
	(step).i_inm = NULL; \
	IN_NEXT_MULTI((step), (inm)); \
}
/*
* Macro for finding the interface (ifnet structure) corresponding to one
* of our IP addresses.
*/
#define INADDR_TO_IFP(addr, ifp) \
	/* struct in_addr addr; */ \
	/* struct ifnet *ifp; */ \
{ \
	register struct in_ifaddr *ia; \
\
	for (ia = in_ifaddr; \
	    ia != NULL && IA_SIN(ia)->sin_addr.s_addr != (addr).s_addr; \
	    ia = ia->ia_next) \
		 continue; \
	(ifp) = (ia == NULL) ? NULL : ia->ia_ifp; \
}



/*
 * Given a pointer to an in_ifaddr (ifaddr),
 * return a pointer to the addr as a sockaddr_in.
 */
#define	IA_SIN(ia) (&(((struct in_ifaddr *)(ia))->ia_addr))

extern int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin, int scrub);

extern void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia);

extern struct in_multi *
in_addmulti(struct in_addr *ap, struct ifnet *ifp);

#endif /* SHAUN_IN_VAL_H_ */
