/*
 * pid.c
 *
 *  Created on: Jan 17, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/page.h>
#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <kernel/bitops.h>
#include <kernel/pid.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>

extern struct kbit_object kbit_obj;

#define BITS_PER_LONG	32
#define BITS_PER_LONG_SHIFT 5

int kernel_id_map_init(struct kid_object *obj, int size)
{
	if (size < PAGE_SIZE){
		obj->kid_map = kmalloc(size);
		assert(obj->kid_map != NULL);
		memset((void *)obj->kid_map, 0xFF, size);
		obj->offset = 0;
		obj->size = size;
	} else {
		obj->kid_map = (unsigned long *)get_zero_page();
		memset((void *)obj->kid_map, 0xFF, PAGE_SIZE);
		obj->offset = 0;
		obj->size = PAGE_SIZE;
	}

	return 0;
}



int kernel_id_map_destroy(struct kid_object *obj)
{
	if (obj->size < PAGE_SIZE){
		kfree((void *)obj->kid_map);
	} else {
		free_page((unsigned long)obj->kid_map);
	}

	return 0;
}


int get_unused_id(struct kid_object *obj)
{
	int index = -1;
	int retval = -1;
AGAIN:
	index = kbit_obj.find_first_bit(obj->kid_map, obj->offset);
	if (index == -1){
		obj->offset++;
		if (obj->offset == (obj->size / 4))
			obj->offset = 0;
		goto AGAIN;
	}
	kbit_obj.clear_bit(index, (unsigned long*)(obj->kid_map + obj->offset));

	retval = obj->offset * BITS_PER_LONG + index;

	if (index == 31){
		obj->offset++;
		if (obj->offset == (obj->size / 4))
			obj->offset = 0;
	}

	return retval;

}


int put_id(struct kid_object *obj, int pid)
{
	int offset = (pid >> BITS_PER_LONG_SHIFT);
	int  index = pid % BITS_PER_LONG;
	kbit_obj.set_bit(index, (unsigned long *)(obj->kid_map + offset));

	if (offset < obj->offset)
		obj->offset = offset;

	return 0;

}

int resv_id(struct kid_object *obj, int id)
{
	int offset = (id >> BITS_PER_LONG_SHIFT);
	int  index = id % BITS_PER_LONG;
	kbit_obj.clear_bit(index, (unsigned long*)(obj->kid_map + offset));
	return 0;
}

int kid_dup(struct kid_object *dobj, struct kid_object *sobj)
{
	if (dobj == NULL || sobj == NULL)
		return 0;

	dobj->offset = sobj->offset;
	dobj->size = sobj->size;

	memcpy((void *)dobj->kid_map, (void *)sobj->kid_map, dobj->size);
	return 0;
}


