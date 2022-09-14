
; boot.asm
;
; Copyright (c) 2012 Shaun Yuan
;
; note: our kernel is a multiboot kernel
; in boot phase, we setup a temp gdt, and make a temp map
; our kernel space is from 0xC0000000 to 0xFFFFFFFF,
; that is the last 1G in 4G liner space,
; we also tell grub to load kernel in physical address 0x100000


MBOOT_PAGE_ALIGN    equ 1<<0    				; Load kernel and modules on a page boundary
MBOOT_MEM_INFO      equ 1<<1    				; Provide your kernel with memory info
MBOOT_GRAPHIC		equ	1<<2
MBOOT_AOUT_KLUDGE	equ 1<<16					; tell grub to load in specific address in mb header
MBOOT_HEADER_MAGIC  equ 0x1BADB002 				; Multiboot Magic value
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO |MBOOT_AOUT_KLUDGE |0x04
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

[BITS 32]                       ; All instructions should be 32-bit.
[GLOBAL mboot]                  ; Make 'mboot' accessible from C.
[extern global_pg_dir]			; defined in page.c
[GLOBAL __PAGE_OFFSET]
[GLOBAL start]                  ; Kernel entry point.
[EXTERN kmain]                  ; This is the entry point of our C code
[EXTERN pg0]					; pg0 is at the _end
[EXTERN _code]                   ; Start of the '.text' section.
[EXTERN _bss]                    ; Start of the .bss section.
[EXTERN _end]                    ; End of the last loadable section.
[extern video_data]
[extern install_video]
__PAGE_OFFSET		equ 0xC0000000
[section .text]
mboot:
  dd  MBOOT_HEADER_MAGIC        ; GRUB will search for this value on each
                                ; 4-byte boundary in your kernel file
  dd  MBOOT_HEADER_FLAGS        ; How GRUB should load your file / settings
  dd  MBOOT_CHECKSUM            ; To ensure that the above values are correct

  dd  mboot - __PAGE_OFFSET     ; Location of this descriptor
  dd  _code - __PAGE_OFFSET     ; Start of kernel '.text' (code) section.
  dd  _bss  - __PAGE_OFFSET     ; End of kernel '.data' section.
  dd  _end  - __PAGE_OFFSET     ; End of kernel.
  dd  start - __PAGE_OFFSET     ; Kernel entry point (initial EIP).
  dd	0
  dd	0
  dd	0
  dd	0


init_pic:
	mov	al,	0x11				;initializetion command, 0x11
	out	0x20,	al				;master command,
	call delay					;delay
	out	0xa0,	al				;slave command
	call	delay
	mov	al,	0x20				;vector offset, start from 32, 32-39 is for master irq
	out	0x21,	al				;master data port
	call	delay
	mov	al,	0x28				;slave offset, start form 40, 40-47 is for slave irq
	out	0xa1,	al				;slave data port
	call	delay
	mov	al,	0x04				;tell master, there is a slave pic at irq2
	out	0x21,	al
	call	delay
	mov	al,	0x02				;tell Slave PIC its cascade identity
	out	0xa1,	al
	call	delay
	mov	al,	0x01				;8086 mode
	out	0x21,	al
	call	delay
	out	0xa1,	al
	call	delay
	mov	al,	0xff				;set master irq mask, mask all interrupt except for slave pic
	out	0x21,	al
	call	delay
	ret

delay:
	jmp	.done
.done:
	ret

start:
 	push    ebx                  		 ; Load multiboot header location
	push	eax
	;we clear the second page,
	;first and second page was reserved for
	;internal use,
	;first page stores bios data
	;and second page used for int86 real mode code
	;and stack
	xor	eax,	eax
	mov edi,	0x1000
	mov	ecx,	0x400
	cld
	rep	stosd

	lgdt	[gdt_ptr - __PAGE_OFFSET]	; load our new gdt
	jmp	0x08:.next - __PAGE_OFFSET
.next:
	mov	ax,	0x10
	mov	ds,	ax
	mov	es,	ax
	mov	ss,	ax
	mov	fs,	ax
	mov	gs,	ax

	call init_pic

 	mov	dx,	0x3f2						;kill monotor
	xor	al,	al
	out	dx,	al

	mov	edi,	pg0 - __PAGE_OFFSET		; pg0 is at _end
	mov	ebx,	global_pg_dir - __PAGE_OFFSET
	lea	ecx,	[edi + 0x07]
	mov	[ebx],	ecx						; set pde
	xor	eax,	eax
	mov	ecx,	0x400
	mov eax,	0x07
.loop:									; fill pg0
	stosd
	;mov	[edi],	eax
	;add	edi,	0x04
	add	eax,	0x1000
	loop	.loop

next:									;map kernel space from 0xC0000000 - 0xC03fffff to first 4M
	mov	eax,	__PAGE_OFFSET
	shr	eax,	20
	mov	ecx,	[ebx]
	mov	[eax+ebx],	ecx


	mov	cr3,	ebx
	mov	eax,	cr0
	or	eax,	0x80000000
	mov	cr0,	eax						; enable paging

	jmp	0x08:.next						; flush prefetch and eip

.next:
	mov	ebp,	esp
	mov esp,	pg0+4096				; kernel stack is at pg0
	push dword [ebp+4]					; multiboot header
	push dword [ebp]					; multiboot magic
  	cli
  	call kmain                   		; call our main() function.
  	jmp $



[GLOBAL gdt_ptr]
gdt_ptr:
	dw 8*3
	dd boot_gdt_table - __PAGE_OFFSET



boot_gdt_table:
	dw	0
	dw	0
	dw	0
	dw	0

	dw 0xFFFF
	dw 0x0000
	dw 0x9a00
	dw 0x00CF					; kernel 4GB code at 0x00000000

	dw 0xFFFF
	dw 0x0000
	dw 0x9200
	dw 0x00CF					; kernel 4GB data at 0x00000000

	dw 0xffff					; real mode code
	dw 0x0000
	dw 0x9b00
	dw 0x0000

	dw 0xffff					; real mode data
	dw 0x0000
	dw 0x9300
	dw 0x0000


[section .bss]
;global_pg_dir:					; coresponding to swapper_pg_dir in linux kernel
