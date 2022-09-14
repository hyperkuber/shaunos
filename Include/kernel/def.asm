;
; def.asm
;
; Copyright (c) 2012 Shaun Yuan
;
;

%ifndef SHAUN_DEF_ASM_
%define SHAUN_DEF_ASM_

BOOTSEG	EQU	0x07c0

INITSEG EQU 0x9000

SETUPSEG EQU 0x9020

KERNSEG EQU 0x1000

SETUP_SECTOR_START EQU 0x02

SETUP_LEN EQU 0x02

KERN_SECTOR_START EQU (SETUP_LEN + 1)

KERN_LEN EQU 0x24

SECTORS_PER_TRACK EQU 0x12

KERNEL_CS	equ	0x08
KERNEL_DS	equ	0x10

KERNEL_STACK	equ	(1024*1024+4096)


%endif