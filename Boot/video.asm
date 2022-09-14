

BITS 16
org	0x1000
video_start:
align	4
	mov ax,	cs
	mov	es,	ax
	mov ds,	ax

	push	es
	mov di,	word 0x100
	mov	ax,	word 0x4f00
	int 0x10
	pop	es

	cmp	ax, word 0x004f
	jne	fail
	;get video mode ptr
	;VideoModePtr is present for seg:offset
	mov si,	word [0x100 + 0x0e]
	;so, get segment
	push es
	mov ax, word [0x100 + 0x10]
	mov es,	ax

loop:
	;now,es:si points to mode numbers list
	;get first mode number
	mov	dx,	word [es:si]

	pop	es
	;check if we reach the end?
	cmp dx, 0xffff
	je	done
	;si points to next number
	add	si, 2

	mov	cx,	dx
	mov	ax, 0x4f01
	mov	di,	0x1300
	int 0x10
	;succeed?
	cmp al,	0x4f
	jne	fail
	cmp	ah, 0x00
	jne	loop
	;get mode attributes
	mov	ax, word	[0x1300]
	;available?
	bt	ax, 0
	jnc	loop
	;graphic mode?
	bt	ax,	4
	jnc	loop
	;Linear frame buffer mode is available?
	bt	ax,	7
	jnc	loop

	mov	ax,	word	[0x1300 + 0x12]
	cmp	ax,	640
	jne	loop
	mov	ax,	word	[0x1300 + 0x14]
	cmp	ax,	400
	jne	loop

	xor	ax,	ax
	mov	al,	byte	[0x1300 + 0x19]
	cmp	ax,		8
	jne	loop

	;switch to the mode
	mov	bx,	0x105
	or	bx,	0x4000	;clear the buffer
	mov	ax,	0x4f02
	int 0x10
	;get liner frame buffer address
	mov	ecx,	dword	[0x1300 + 0x28]

	jmp loop

fail:
	jmp $

done:
	jmp $

desired_xres	dw	640
desired_yres	dw	400
desired_bpp	dw	8

mode_info dw 0x1300

vesa_info dw 0x1100




