/*
 * bitops.h
 *
 *  Created on: Dec 13, 2012
 *      Author: root
 */

#ifndef BITOPS_H_
#define BITOPS_H_
#include <kernel/kernel.h>

struct kbit_object {
	struct kernel_object kobj;
	int (*find_first_bit)(volatile unsigned long *addr, int offset);
	int (*clear_bit)(int nr, volatile unsigned long *addr);
	int (*set_bit)(int nr, volatile unsigned long *addr);
};

int set_bit(int nr, volatile unsigned long *addr);
int find_first_bit(volatile unsigned long *addr, int offset);
int clear_bit(int nr, volatile unsigned long *addr);
int get_bit(int nr, volatile unsigned long *addr);
#endif /* BITOPS_H_ */
