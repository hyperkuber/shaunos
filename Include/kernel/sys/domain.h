/*
 * domain.h
 *
 *  Created on: Aug 2, 2013
 *      Author: root
 */

#ifndef SHAUN_DOMAIN_H_
#define SHAUN_DOMAIN_H_

#include <kernel/mbuf.h>
#include <kernel/sys/socket.h>
struct protosw;


struct domain {
	int dom_family;
	char *dom_name;
	void (*dom_init)(void);
	int (*dom_externalize)(struct mbuf *m);
	int (*dom_dispose)(struct mbuf *m);
	struct protosw *dom_protosw, *dom_protoswNPROTOSW;
	struct domain *dom_next;
	int (*dom_rtattach)(void **, int);
	int dom_rtoffset;
	int dom_maxrtkey;
};


extern struct domain *domains;

extern struct protosw *
pffindtype(int family, int type);

extern struct protosw *
pffindproto(int family, int protocol, int type);
extern void
pfctlinput(int cmd, struct sockaddr *sa);

extern void
domaininit();

#endif /* SHAUN_DOMAIN_H_ */
