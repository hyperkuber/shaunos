/*
 * netisr.h
 *
 *  Created on: Jun 21, 2013
 *      Author: root
 */

#ifndef SHAUN_NETISR_H_
#define SHAUN_NETISR_H_

#include <kernel/kthread.h>

#define	NETISR_RAW	0		/* same as AF_UNSPEC */
#define	NETISR_IP	2		/* same as AF_INET */
#define	NETISR_IMP	3		/* same as AF_IMPLINK */
#define	NETISR_NS	6		/* same as AF_NS */
#define	NETISR_ISO	7		/* same as AF_ISO */
#define	NETISR_CCITT	10		/* same as AF_CCITT */
#define	NETISR_ARP	18		/* same as AF_LINK */
#define NETISR_ACT (1<<31)

extern volatile int	netisr_mask;


void* netisr(void * args);
kthread_u_t *netisr_thread;

#define	schednetisr(anisr)	\
	{ netisr_mask |= (1<<(anisr)); 	\
	if ((netisr_mask & NETISR_ACT) == 0 ) 		\
		setsoftnet(); }

#define	setsoftnet()	(pt_wakeup(&(netisr_thread->kthread)))

#endif /* SHAUN_NETISR_H_ */
