/*
 * pid.h
 *
 *  Created on: Jan 17, 2013
 *      Author: root
 */

#ifndef SHAUN_PID_H_
#define SHAUN_PID_H_
#include <kernel/kernel.h>

struct kid_object {
	unsigned long *kid_map;
	unsigned long offset;
	unsigned long size;
};
extern int kernel_id_map_init(struct kid_object *obj, int size);
extern int kernel_id_map_destroy(struct kid_object *obj);
extern int get_unused_id(struct kid_object *obj);
extern int put_id(struct kid_object *obj, int pid);
extern int resv_id(struct kid_object *obj, int id);
extern int kid_dup(struct kid_object *dobj, struct kid_object *sobj);
#endif /* SHAUN_PID_H_ */
