/*
 * vesa.c
 *
 *  Created on: Feb 18, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/int86.h>
#include <kernel/vesa.h>
#include <kernel/mm.h>
#include <kernel/page.h>
#include <string.h>
#include <kernel/assert.h>


static void
fix_pointer (void *to, void *from, unsigned long *ptr)
{
	unsigned short off = ((unsigned short*) ptr) [0];
	unsigned short segm = ((unsigned short*) ptr) [1];

	*ptr = off + ((unsigned long) segm << 4);
	if ((void*) *ptr >= from && (void*) *ptr < from+512)
		*ptr += to - from;
	/* debug_printf ("%04x:%04x -> %08x\n", segm, off, *ptr); */
}

int
vesa_get_info (vesa_info_t *i)
{
	int86_regs_t reg;
	unsigned char *data = (unsigned char*) alloc_pages(ALLOC_LOWMEM | ALLOC_ZEROED, 0);

	reg.x.ax = 0x4f00;
	reg.x.di = 0;
	reg.x.es = PA(data) >> 4;
	memcpy (data, "VBE2", 4);

	int86 (0x10, &reg, &reg);
	if (reg.x.ax != 0x004f)
		return -1;

	memcpy (i, data, sizeof (*i));
	fix_pointer (i, data, (unsigned long*) &i->oem_string);
	fix_pointer (i, data, (unsigned long*) &i->modes);
	fix_pointer (i, data, (unsigned long*) &i->vendor_name);
	fix_pointer (i, data, (unsigned long*) &i->product_name);
	fix_pointer (i, data, (unsigned long*) &i->product_rev);

	//free_pages(1, (unsigned long)data);
	return 0;
}


int
vesa_get_mode_info (unsigned short mode, vesa_mode_info_t *i)
{
	int86_regs_t reg;
	unsigned char *data = (unsigned char*) alloc_pages(ALLOC_LOWMEM | ALLOC_ZEROED, 0);
	assert(data != NULL);
	reg.x.ax = 0x4f01;
	reg.x.cx = mode;
	reg.x.di = 0;
	reg.x.es = PA(data) >> 4;

	int86 (0x10, &reg, &reg);
	if (reg.x.ax != 0x004f)
		return -1;

	memcpy(i, (void*) data, sizeof (*i));
	free_pages(1, (unsigned long)data);
	return 0;
}

unsigned short
vesa_get_mode ()
{
	int86_regs_t reg;

	reg.x.ax = 0x4f03;
	int86 (0x10, &reg, &reg);
	if (reg.x.ax != 0x004f)
		return -1;
	return reg.x.bx;
}

int
vesa_set_mode (unsigned short mode)
{
	int86_regs_t reg;

	reg.x.ax = 0x4f02;
	reg.x.bx = mode & ~0x0800; /* no CRTC values */
	int86 (0x10, &reg, &reg);
	if (reg.x.ax != 0x004f)
		return -1;
	return 0;
}

//int
//vesa_set_palette (int first, int count, void *data)
//{
//	int86_regs_t reg;
//
//	reg.x.ax = 0x4f09;
//	reg.x.bx = 0;
//	reg.x.cx = count;
//	reg.x.dx = first;
//	reg.x.di = 0;
//	reg.x.es = LOWMEM >> 4;
//	memcpy ((void*) LOWMEM, data, 4*count);
//
//	int86 (0x10, &reg, &reg);
//	if (reg.x.ax != 0x004f)
//		return 0;
//	return 1;
//}



















