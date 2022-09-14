 ;
 ;trap.asm
 ;
 ; Copyright (c) 2012 Shaun Yuan
 ;
 ;




[BITS 32]

global divide_error,device_not_available,page_fault
global debug,nmi,int3,overflow,bounds,invalid_op
global double_fault,coprocessor_segment_overrun
global invalid_TSS,segment_not_present,stack_segment
global general_protection,coprocessor_error,reserved


extern 	do_divide_error,	do_int3, 	do_nmi, do_int3,	do_overflow,	do_device_not_available
extern	do_bounds,	do_invalid_op,	do_coprocessor_segment_overrun,		do_coprocessor_error
extern	do_reserved,	do_double_fault,	do_invalid_TSS,
extern	do_segment_not_present,	do_stack_segment,	do_general_protection,	do_page_fault

no_error_code:
	xchg	[esp],	eax
	push 	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	push	es
	push	ds
	push	fs
	push	gs
	push	0x1				;fake error code
	push	esp				;struct pointer
	mov		edx,	0x10
	mov		ds,		dx
	mov		es,		dx
	mov		fs,		dx
	call	eax
	add		esp,	0x08
	pop		gs
	pop		fs
	pop		ds
	pop		es
	pop		ebp
	pop		edi
	pop		esi
	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
	iret


divide_error:
	push do_divide_error
	jmp no_error_code
debug:
	push	do_int3
	jmp	no_error_code

nmi:
	push	do_nmi
	jmp	no_error_code

int3:
	push	do_int3
	jmp	no_error_code

overflow:
	push	do_overflow
	jmp	no_error_code

bounds:
	push	do_bounds
	jmp	no_error_code
invalid_op:
	push	do_invalid_op
	jmp	no_error_code

coprocessor_segment_overrun:
	push do_coprocessor_segment_overrun
	jmp	no_error_code

reserved:
	push	do_reserved
	jmp	no_error_code

page_fault:
	push	do_page_fault
	jmp		no_error_code

device_not_available:
	push do_device_not_available
	jmp 	no_error_code

coprocessor_error:
	push do_coprocessor_error
	jmp		no_error_code

with_error_code:
	xchg	[esp +4],	eax
	xchg	[esp],	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	push	es
	push	ds
	push	fs
	push	gs
	push	eax				;error code
	push	esp
	mov		eax,	0x10
	mov		ds,		ax
	mov		es,		ax
	mov		fs,		ax
	call	ebx
	add		esp,	0x08
	pop		gs
	pop		fs
	pop		ds
	pop		es
	pop		ebp
	pop		edi
	pop		esi
	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
	iret

double_fault:
	push	do_double_fault
	jmp		with_error_code

invalid_TSS:
	push	do_invalid_TSS
	jmp		with_error_code

segment_not_present:
	push do_segment_not_present
	jmp		with_error_code

stack_segment:
	push	do_stack_segment
	jmp		with_error_code

general_protection:
	push	do_general_protection
	jmp		with_error_code

