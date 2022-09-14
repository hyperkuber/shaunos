/*
 * irq.h
 *
 *  Created on: Feb 24, 2013
 *      Author: root
 */

#ifndef IRQ_H_
#define IRQ_H_

#define IRQ_REPLACE (1<<1)
#define IRQ_SHARED	(1<<2)


void irq_init();
int register_irq(int vector, unsigned long handler, void *data, int flag);

void enable_irq(int num);

void pic_send_eoi(unsigned char irq);

#endif /* IRQ_H_ */
