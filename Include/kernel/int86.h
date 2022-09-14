/*
 * int86.h
 *
 *  Created on: Feb 18, 2013
 *      Author: root
 */

#ifndef SHAUN_INT86_H_
#define SHAUN_INT86_H_


typedef union _int86_regs {
	struct _x {
		unsigned short ax;
		unsigned short bx;
		unsigned short cx;
		unsigned short dx;
		unsigned short si;
		unsigned short di;
		unsigned short bp;
		unsigned short es;
		unsigned short ds;
		unsigned short flags;
	} x;
	struct _h {
		unsigned char al, ah;
		unsigned char bl, bh;
		unsigned char cl, ch;
		unsigned char dl, dh;
	} h;
} int86_regs_t;

int int86_init();
void int86 (int intnum, int86_regs_t *inregs, int86_regs_t *outregs);

#endif /* SHAUN_INT86_H_ */
