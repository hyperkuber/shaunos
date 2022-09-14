;
; trap.asm
;
; Copyright (c) 2012 Shaun Yuan
;
;

[BITS 32]


extern	syscall_table
extern nr_syscalls
extern need_reschedule
extern schedule
global	_isr_syscall
global	thread_stub
extern dispatch_isr
extern dispatch_irq

thread_stub:
	pop	gs
	pop	fs
	pop	ds
	pop	es
	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax
	iret


%macro	save_all 0
	push	eax
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp

	push 	es
	push	ds
	push	fs
	push	gs
%endmacro


%macro restore_all 0
	pop	gs
	pop	fs
	pop	ds
	pop	es
	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax
	add	esp,	8
	iret
%endmacro

%macro generate_irq 2
global _irq%1
_irq%1:
	push 0	;fake error code
	push %2	;vector number
	jmp general_irq_routine
%endmacro

generate_irq 0, 0x20
generate_irq 1, 0x21
generate_irq 2, 0x22
generate_irq 3, 0x23
generate_irq 4, 0x24
generate_irq 5, 0x25
generate_irq 6, 0x26
generate_irq 7, 0x27
generate_irq 8, 0x28
generate_irq 9, 0x29
generate_irq a, 0x2a
generate_irq b, 0x2b
generate_irq c, 0x2c
generate_irq d, 0x2d
generate_irq e, 0x2e
generate_irq f, 0x2f

general_irq_routine:
	save_all
	push	esp
	mov	ax,	ss
	mov	ds,	ax
	mov	es,	ax

	call dispatch_irq
	add	esp,	4	;skip param
	cmp dword [need_reschedule],	0
	jz	.next
	call schedule
.next:
	restore_all





%macro generate_isr_no_err 2
global _isr_%1
_isr_%1 :
	push 0	;fake error code
	push %2	;vector number
	jmp general_isr_routine
%endmacro

%macro generate_isr_with_err 2
global _isr_%1
_isr_%1 :
	push %2	;vector number
	jmp general_isr_routine
%endmacro

general_isr_routine:
	save_all
	push	esp
	mov	ax,	ss
	mov	ds,	ax
	mov	es,	ax
	call dispatch_isr
	add	esp,	4
	restore_all

generate_isr_no_err divide_error, 0x00
generate_isr_no_err debug, 0x01
generate_isr_no_err	nmi, 0x02
generate_isr_no_err int3, 0x03
generate_isr_no_err overflow, 0x04
generate_isr_no_err bounds, 0x05
generate_isr_no_err invalid_op, 0x06
generate_isr_no_err device_not_available, 0x07
generate_isr_with_err double_fault, 0x08
generate_isr_no_err coprocessor_segment_overrun, 0x09
generate_isr_with_err invalid_tss, 0x0A
generate_isr_with_err segment_not_present, 0x0B
generate_isr_with_err stack_segment, 0x0C
generate_isr_with_err general_protection, 0x0D
generate_isr_with_err page_fault, 0x0E
generate_isr_no_err	reserved, 0x0F
generate_isr_no_err fpu, 0x10
generate_isr_with_err align_check, 0x11
generate_isr_no_err mechine_check, 0x12
generate_isr_no_err simd_error, 0x13
;20-31 is reserved

_isr_syscall:
	push 0		;error code
	push 0x80	;vector
	save_all
	push	esp		;struct pointer

	mov	edx,	0x10
	mov	es,	dx
	mov	ds,	dx
	call	[syscall_table+eax*4]
	add	esp,	0x04
syscall_exit:
	pop gs
	pop	fs
	pop	ds
	pop	es
	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	add	esp,	0x0C	;skip	eax, as it stores return value from syscall
	iret












