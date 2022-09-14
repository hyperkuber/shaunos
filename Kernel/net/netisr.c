/*
 * netisr.c
 *
 *  Created on: Jun 21, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/net/netisr.h>

volatile int netisr_mask;

/*
 * this is the network kernel thread
 * routine
 */
void* netisr(void * args)
{

	while (1){

		netisr_mask |= NETISR_ACT;

		if (netisr_mask & (1<<NETISR_IP)){
			void ipintr(void);

			netisr_mask &= ~(1<<NETISR_IP);
			ipintr();
		}
		if (netisr_mask & (1<<NETISR_ARP)) {
			void arpintr(void);

			netisr_mask &= ~(1<<NETISR_ARP);
			arpintr();
		}

		netisr_mask &= ~NETISR_ACT;
		schedule();
	}
	return NULL;
}
