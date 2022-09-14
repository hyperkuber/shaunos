/*
 * segment.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_SEGMENT_H_
#define SHAUN_SEGMENT_H_

#include <kernel/types.h>
#include <kernel/kernel.h>


#define NR_GDTENTRIES 256



struct segment_descriptor {
	ushort_t limit_low;	packed		//segment limit
	uint_t	base_low:24 packed;		//segment base address
	uint_t	type:4 packed;			//segment type
	uint_t	system:1 packed;		//descriptor type 0=system 1=code or data
	uint_t	dpl:2 packed;			//descriptor privilege level
	uint_t	present:1 packed;		//segment present
	uint_t	limit_high:4 packed;	//segment limit
	uint_t	avl:1 packed;			//available for use by software
	uint_t	reserved:1 packed;		//reserved
	uint_t	db:1 packed;			//default operation size (0=16 bit segment, 1=32bit segment
	uint_t	granularity:1 packed;	//1=page size,
	uint_t	base_high:8 packed;		//base address
};


struct gdt_ptr {
	ushort_t limit;
	uint_t	addr;
}packed;


void init_gdt(void);

#endif /* SHAUN_SEGMENT_H_ */
