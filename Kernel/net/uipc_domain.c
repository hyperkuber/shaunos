/*
 * uipc_domain.c
 *
 *  Created on: Oct 12, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/timer.h>
#include <kernel/sys/domain.h>
#include <kernel/sys/socket.h>
#include <kernel/sys/protosw.h>


struct domain *domains;


struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp != NULL; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return NULL;
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	return NULL;
}

struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	struct protosw *pr;
	struct protosw *maybe = NULL;

	if (family == 0)
		return NULL;
	for (dp = domains; dp != NULL; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return NULL;
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == (struct protosw *)0)
			maybe = pr;
	}
	return (maybe);
}

#define __CONCAT(a, b)	a##b

#define	ADDDOMAIN(x)	{ \
	extern struct domain __CONCAT(x,domain); \
	__CONCAT(x,domain.dom_next) = domains; \
	domains = &__CONCAT(x,domain); \
}

void
pfslowtimo(void *arg)
{
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();

}

void
pffasttimo(void *arg)
{
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();

}

void
pfctlinput(int cmd, struct sockaddr *sa)
{
	struct domain *dp;
	struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, (caddr_t)0);
}


void
domaininit()
{
	struct domain *dp;
	struct protosw *pr;

	LOG_INFO("in");
	ADDDOMAIN(inet);
	ADDDOMAIN(route);
	for (dp = domains; dp != NULL; dp = dp->dom_next) {
		if (dp->dom_init)
			(*dp->dom_init)();
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_init)
				(*pr->pr_init)();
	}

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	register_timer(HZ/5, TIMER_FLAG_PERIODIC | TIMER_FLAG_ENABLED, pffasttimo, 0);
	register_timer(HZ/2, TIMER_FLAG_PERIODIC | TIMER_FLAG_ENABLED, pfslowtimo, 0);
	LOG_INFO("end");
}
