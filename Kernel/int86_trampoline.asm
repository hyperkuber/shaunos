;
; int86_trampoline.asm
;
;  Created on: Feb 18, 2013
;      Author: Shaun Yuan
;	Copyright (c) 2013 Shaun Yuan


ORG	0x1000
BITS 32

	pusha

	mov	eax,	esp
	mov	dword [saved_stack],	eax

	mov	esp,	0x1fec		;0x2000-sizeof(int86_regs_t)
	xor	eax,	eax
	mov	ebx,	eax
	mov	ecx,	eax
	mov	edx,	eax
	mov	esi,	eax
	mov	edi,	eax
	mov	ebp,	eax
	; disable paging
	mov	eax,	cr0
	and eax,	0x7FFFFFFF
	mov	cr0,	eax

	jmp	real_mode_cs:code16
code16:
	BITS 16
	;enter real mode
	mov	eax,	cr0
	and	ax,	0xfe
	mov	cr0,	eax

	jmp 0x0:.next			;reload cs
.next:
	xor	eax,	eax
	mov	ss,	ax
	mov	fs,	ax
	mov	gs,	ax

	pop	ax
	pop	bx
	pop	cx
	pop	dx
	pop	si
	pop	di
	pop	bp
	pop	es
	pop	ds
	add	sp,	2			;now, sp points to 0x2000


	times	($$+0x60 - $) db	0x90	;nop
	; code0 is at 0x1060
code0:					;put real mode code here, int
	nop
	nop
	nop
	nop
code1:
	nop
	nop
	nop
	nop


	pushf
	push ds
	push es
	push bp
	push di
	push si
	push dx
	push cx
	push bx
	push ax

	;enter protect mode
	mov	eax,	cr0
	or	eax,	0x01
	mov	cr0,	eax
	jmp 0x08:.next	; load kernel cs
.next:
BITS 32
	mov	eax, 0x10
	mov	ds,	ax
	mov	ss,	ax
	mov	es,	ax
	mov	fs,	ax
	mov	gs,	ax
	;enable paging
	mov	eax,	cr0
	or	eax,	0x80000000
	mov	cr0,	eax
	;should we flush tlb here?
	mov	eax,	dword	[saved_stack]
	mov	esp,	eax

	popa
	ret
align 4
saved_stack	dd	0
real_mode_cs	equ 6<<3
real_mode_ds	equ	7<<3
