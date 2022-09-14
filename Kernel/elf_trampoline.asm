
BITS	32

	pusha
	lea	eax, [esp+0x20]
	mov	ecx,	[eax+0x04]
	mov	edx,	[eax]
	mov	eax,	0x32
	int	0x80
	mov	[esp+0x20],	eax
	popa	
	ret	$4
	nop
	nop
	nop
	nop
