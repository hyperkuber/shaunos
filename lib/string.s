	.file	"string.c"
	.comm	run_queue,40,32
	.comm	wait_queue,40,32
	.text
.globl strcmp
	.type	strcmp, @function
strcmp:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	je	.L2
	cmpl	$0, 12(%ebp)
	jne	.L3
.L2:
	movl	$-1, %eax
	jmp	.L4
.L3:
	movl	8(%ebp), %eax
	movl	%eax, -8(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -4(%ebp)
	jmp	.L5
.L7:
	addl	$1, -8(%ebp)
	addl	$1, -4(%ebp)
.L5:
	movl	-8(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	je	.L6
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	je	.L6
	movl	-8(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	cmpb	%al, %dl
	je	.L7
.L6:
	movl	-8(%ebp), %eax
	movzbl	(%eax), %eax
	movsbl	%al, %edx
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	movsbl	%al, %eax
	movl	%edx, %ecx
	subl	%eax, %ecx
	movl	%ecx, %eax
.L4:
	leave
	ret
	.size	strcmp, .-strcmp
.globl strncmp
	.type	strncmp, @function
strncmp:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	je	.L10
	cmpl	$0, 12(%ebp)
	jne	.L11
.L10:
	movl	$-1, %eax
	jmp	.L12
.L11:
	movl	8(%ebp), %eax
	movl	%eax, -8(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -4(%ebp)
	jmp	.L13
.L15:
	addl	$1, -8(%ebp)
	addl	$1, -4(%ebp)
.L13:
	cmpl	$0, 16(%ebp)
	setne	%al
	subl	$1, 16(%ebp)
	testb	%al, %al
	je	.L14
	movl	-8(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	je	.L14
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	je	.L14
	movl	-8(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	cmpb	%al, %dl
	je	.L15
.L14:
	movl	-8(%ebp), %eax
	movzbl	(%eax), %eax
	movsbl	%al, %edx
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	movsbl	%al, %eax
	movl	%edx, %ecx
	subl	%eax, %ecx
	movl	%ecx, %eax
.L12:
	leave
	ret
	.size	strncmp, .-strncmp
.globl strcat
	.type	strcat, @function
strcat:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	je	.L18
	cmpl	$0, 12(%ebp)
	jne	.L19
.L18:
	movl	8(%ebp), %eax
	jmp	.L20
.L19:
	movl	8(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %ecx
	movl	-16(%ebp), %edx
	movl	$0, %eax
	movl	%ecx, %esi
	movl	%edx, %edi
#APP
# 52 "string.c" 1
	cld
	1:scasb
	jnz 1b
	decl %edi
	2:lodsb
	stosb
	testb %al, %al
	jnz 2b
	
# 0 "" 2
#NO_APP
	movl	%edi, %edx
	movl	%esi, %ecx
	movl	%ecx, -12(%ebp)
	movl	%edx, -16(%ebp)
	movl	-16(%ebp), %eax
.L20:
	addl	$16, %esp
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	strcat, .-strcat
.globl strncat
	.type	strncat, @function
strncat:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	je	.L23
	cmpl	$0, 12(%ebp)
	je	.L23
	cmpl	$0, 16(%ebp)
	jne	.L24
.L23:
	movl	8(%ebp), %eax
	jmp	.L25
.L24:
	movl	8(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %ebx
	movl	-20(%ebp), %ecx
	movl	$0, %eax
	movl	16(%ebp), %edx
	movl	%ebx, %esi
	movl	%ecx, %ebx
	movl	%ebx, %edi
	movl	%edx, %ecx
#APP
# 76 "string.c" 1
	cld
	1:scasb
	jnz 1b
	decl %edi
	2:decl %ecx
	js 3f
	lodsb
	stosb
	testb %al, %al
	jnz 2b
	3:xorl %eax, %eax
	stosb
	
# 0 "" 2
#NO_APP
	movl	%ecx, %edx
	movl	%edi, %ebx
	movl	%esi, -16(%ebp)
	movl	%ebx, -20(%ebp)
	movl	%edx, 16(%ebp)
	movl	8(%ebp), %eax
.L25:
	addl	$16, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	strncat, .-strncat
.globl strlen
	.type	strlen, @function
strlen:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	jne	.L28
	movl	$-1, %eax
	jmp	.L29
.L28:
	movl	8(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	$0, -8(%ebp)
	movl	$0, %eax
	movl	-12(%ebp), %edx
	movl	%edx, %edi
#APP
# 111 "string.c" 1
	cld
	1:scasb
	jnz 1b
	
# 0 "" 2
#NO_APP
	movl	%edi, %edx
	movl	%edx, -8(%ebp)
	movl	-12(%ebp), %eax
	movl	-8(%ebp), %edx
	movl	%edx, %ecx
	subl	%eax, %ecx
	movl	%ecx, %eax
	subl	$1, %eax
.L29:
	addl	$16, %esp
	popl	%edi
	popl	%ebp
	ret
	.size	strlen, .-strlen
.globl strnlen
	.type	strnlen, @function
strnlen:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$20, %esp
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	strlen
	movl	%eax, -4(%ebp)
	movl	12(%ebp), %eax
	cmpl	-4(%ebp), %eax
	jb	.L32
	movl	-4(%ebp), %eax
	jmp	.L33
.L32:
	movl	12(%ebp), %eax
.L33:
	leave
	ret
	.size	strnlen, .-strnlen
.globl memcmp
	.type	memcmp, @function
memcmp:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	cmpl	$0, 16(%ebp)
	je	.L36
	movl	8(%ebp), %eax
	movl	%eax, -8(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -4(%ebp)
.L39:
	movl	-8(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	cmpb	%al, %dl
	setne	%al
	addl	$1, -8(%ebp)
	addl	$1, -4(%ebp)
	testb	%al, %al
	je	.L37
	subl	$1, -8(%ebp)
	movl	-8(%ebp), %eax
	movzbl	(%eax), %eax
	movzbl	%al, %edx
	subl	$1, -4(%ebp)
	movl	-4(%ebp), %eax
	movzbl	(%eax), %eax
	movzbl	%al, %eax
	movl	%edx, %ecx
	subl	%eax, %ecx
	movl	%ecx, %eax
	jmp	.L38
.L37:
	subl	$1, 16(%ebp)
	cmpl	$0, 16(%ebp)
	jne	.L39
.L36:
	movl	$0, %eax
.L38:
	leave
	ret
	.size	memcmp, .-memcmp
.globl memset
	.type	memset, @function
memset:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	subl	$16, %esp
	movl	8(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	12(%ebp), %eax
	imull	$16843009, %eax, %eax
	movl	%eax, -8(%ebp)
	cmpl	$0, 16(%ebp)
	jne	.L42
	movl	8(%ebp), %eax
	jmp	.L43
.L42:
	cmpl	$31, 16(%ebp)
	ja	.L44
	movl	$0, -12(%ebp)
	jmp	.L45
.L46:
	movl	12(%ebp), %eax
	movl	%eax, %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -16(%ebp)
	addl	$1, -12(%ebp)
.L45:
	movl	-12(%ebp), %eax
	cmpl	16(%ebp), %eax
	jb	.L46
	movl	8(%ebp), %eax
	jmp	.L43
.L44:
	movl	-16(%ebp), %eax
	negl	%eax
	andl	$3, %eax
	subl	%eax, 16(%ebp)
	movl	-16(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L49
	cmpl	$1, %eax
	jb	.L48
	cmpl	$2, %eax
	je	.L50
	cmpl	$3, %eax
	je	.L51
	jmp	.L47
.L49:
	movl	12(%ebp), %eax
	movl	%eax, %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -16(%ebp)
.L50:
	movl	12(%ebp), %eax
	movl	%eax, %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -16(%ebp)
.L51:
	movl	12(%ebp), %eax
	movl	%eax, %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -16(%ebp)
.L48:
	movl	16(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L53
	cmpl	$1, %eax
	jb	.L52
	cmpl	$2, %eax
	je	.L54
	cmpl	$3, %eax
	je	.L55
	jmp	.L47
.L52:
	movl	16(%ebp), %eax
	movl	%eax, %ecx
	shrl	$2, %ecx
	movl	-8(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	%edx, %edi
#APP
# 176 "string.c" 1
	cld
	rep; stosl
	
# 0 "" 2
#NO_APP
	jmp	.L47
.L53:
	movl	16(%ebp), %eax
	movl	%eax, %ecx
	shrl	$2, %ecx
	movl	-8(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	%edx, %edi
#APP
# 187 "string.c" 1
	cld
	rep; stosl
	stosb
	
# 0 "" 2
#NO_APP
	jmp	.L47
.L54:
	movl	16(%ebp), %eax
	movl	%eax, %ecx
	shrl	$2, %ecx
	movl	-8(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	%edx, %edi
#APP
# 198 "string.c" 1
	cld
	rep; stosl
	stosw
	
# 0 "" 2
#NO_APP
	jmp	.L47
.L55:
	movl	16(%ebp), %eax
	movl	%eax, %ecx
	shrl	$2, %ecx
	movl	-8(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	%edx, %edi
#APP
# 210 "string.c" 1
	cld
	rep; stosl
	stosw
	stosb
	
# 0 "" 2
#NO_APP
.L47:
	movl	8(%ebp), %eax
.L43:
	addl	$16, %esp
	popl	%edi
	popl	%ebp
	ret
	.size	memset, .-memset
.globl memcpy
	.type	memcpy, @function
memcpy:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$16, %esp
	cmpl	$0, 12(%ebp)
	je	.L58
	cmpl	$0, 8(%ebp)
	jne	.L59
.L58:
	movl	$0, %eax
	jmp	.L60
.L59:
	movl	8(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -16(%ebp)
	cmpl	$0, 16(%ebp)
	jne	.L61
	movl	8(%ebp), %eax
	jmp	.L60
.L61:
	cmpl	$31, 16(%ebp)
	ja	.L62
	movl	-20(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	16(%ebp), %ecx
	movl	%eax, %edi
	movl	%edx, %esi
#APP
# 245 "string.c" 1
	cld
	rep; movsb
	
# 0 "" 2
#NO_APP
	movl	8(%ebp), %eax
	jmp	.L60
.L62:
	movl	8(%ebp), %eax
	negl	%eax
	andl	$3, %eax
	subl	%eax, 16(%ebp)
	movl	8(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L65
	cmpl	$1, %eax
	jb	.L64
	cmpl	$2, %eax
	je	.L66
	cmpl	$3, %eax
	je	.L67
	jmp	.L63
.L65:
	movl	-16(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-20(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -20(%ebp)
	addl	$1, -16(%ebp)
.L66:
	movl	-16(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-20(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -20(%ebp)
	addl	$1, -16(%ebp)
.L67:
	movl	-16(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-20(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, -20(%ebp)
	addl	$1, -16(%ebp)
.L64:
	movl	16(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L69
	cmpl	$1, %eax
	jb	.L68
	cmpl	$2, %eax
	je	.L70
	cmpl	$3, %eax
	je	.L71
	jmp	.L63
.L68:
	movl	-20(%ebp), %edx
	movl	-16(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 269 "string.c" 1
	cld
	rep; movsl
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -20(%ebp)
	movl	%edx, -16(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L63
.L69:
	movl	-20(%ebp), %edx
	movl	-16(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 279 "string.c" 1
	cld
	rep; movsl
	movsb
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -20(%ebp)
	movl	%edx, -16(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L63
.L70:
	movl	-20(%ebp), %edx
	movl	-16(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 290 "string.c" 1
	cld
	rep; movsl
	movsw
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -20(%ebp)
	movl	%edx, -16(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L63
.L71:
	movl	-20(%ebp), %edx
	movl	-16(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 301 "string.c" 1
	cld
	rep; movsl
	movsw
	movsb
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -20(%ebp)
	movl	%edx, -16(%ebp)
	movl	%eax, 16(%ebp)
.L63:
	movl	8(%ebp), %eax
.L60:
	addl	$16, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	memcpy, .-memcpy
.globl strcpy
	.type	strcpy, @function
strcpy:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	subl	$16, %esp
	cmpl	$0, 8(%ebp)
	je	.L74
	cmpl	$0, 12(%ebp)
	jne	.L75
.L74:
	movl	$0, %eax
	jmp	.L76
.L75:
	movl	12(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	movl	8(%ebp), %edx
	movl	%eax, %esi
	movl	%edx, %edi
#APP
# 328 "string.c" 1
	cld
	1: lodsb
	stosb
	testb %al, %al
	jnz 1b
	
# 0 "" 2
#NO_APP
	movl	8(%ebp), %eax
.L76:
	addl	$16, %esp
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	strcpy, .-strcpy
.globl strncpy
	.type	strncpy, @function
strncpy:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$16, %esp
	movl	12(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	8(%ebp), %eax
	movl	%eax, -16(%ebp)
	cmpl	$0, 16(%ebp)
	jne	.L79
	movl	8(%ebp), %eax
	jmp	.L80
.L79:
	movl	-20(%ebp), %edx
	movl	-16(%ebp), %ecx
	movl	16(%ebp), %eax
	movl	%ecx, %ebx
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 354 "string.c" 1
	cld
	1:decl %ecx
	js 2f
	lodsb
	stosb
	testb %al, %al
	jnz 1b
	rep; stosb
	jmp 3f
	2:xor %eax, %eax
	stosb
	3:
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -16(%ebp)
	movl	%edx, -20(%ebp)
	movl	%eax, 16(%ebp)
	movl	8(%ebp), %eax
.L80:
	addl	$16, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	strncpy, .-strncpy
.globl strchr
	.type	strchr, @function
strchr:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%ebx
	subl	$16, %esp
	movl	8(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	12(%ebp), %edx
	movl	-12(%ebp), %eax
	movl	%eax, %esi
	movl	%edx, %ebx
#APP
# 377 "string.c" 1
	cld
	1:lodsb
	cmpb %al, %bl
je 2f
	testb %al, %al
	jnz 1b
	movl $0, %esi
	jmp 3f
	2:decl %esi
	3:
# 0 "" 2
#NO_APP
	movl	%esi, %eax
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	addl	$16, %esp
	popl	%ebx
	popl	%esi
	popl	%ebp
	ret
	.size	strchr, .-strchr
.globl strrchr
	.type	strrchr, @function
strrchr:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%ebx
	subl	$20, %esp
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	strlen
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	movl	8(%ebp), %edx
	leal	(%edx,%eax), %eax
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	movl	-16(%ebp), %edx
	movl	12(%ebp), %ebx
	movl	%eax, %esi
	movl	%edx, %ecx
#APP
# 399 "string.c" 1
	std
	1:lodsb
	cmpb %al, %bl
	je 2f
	decl %ecx
	jnz 1b
	movl $0, %esi
	jmp 3f
	2: incl %esi
	3:
# 0 "" 2
#NO_APP
	movl	%esi, %eax
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	addl	$20, %esp
	popl	%ebx
	popl	%esi
	popl	%ebp
	ret
	.size	strrchr, .-strrchr
.globl strdup
	.type	strdup, @function
strdup:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	strlen
	addl	$1, %eax
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	kmalloc
	movl	%eax, -12(%ebp)
	cmpl	$0, -12(%ebp)
	jne	.L87
	movl	$0, %eax
	jmp	.L88
.L87:
	movl	-16(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	8(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	-12(%ebp), %eax
	movl	%eax, (%esp)
	call	memcpy
	movl	-12(%ebp), %eax
.L88:
	leave
	ret
	.size	strdup, .-strdup
.globl memmove
	.type	memmove, @function
memmove:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$28, %esp
	cmpl	$0, 8(%ebp)
	je	.L91
	cmpl	$0, 12(%ebp)
	je	.L91
	cmpl	$0, 16(%ebp)
	jne	.L92
.L91:
	movl	8(%ebp), %eax
	jmp	.L93
.L92:
	movl	12(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	8(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	8(%ebp), %eax
	cmpl	12(%ebp), %eax
	jbe	.L94
	movl	8(%ebp), %edx
	movl	12(%ebp), %eax
	addl	16(%ebp), %eax
	cmpl	%eax, %edx
	ja	.L94
	movl	16(%ebp), %eax
	addl	%eax, -20(%ebp)
	movl	16(%ebp), %eax
	addl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	negl	%eax
	andl	$3, %eax
	subl	%eax, 16(%ebp)
	movl	-16(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L97
	cmpl	$1, %eax
	jb	.L96
	cmpl	$2, %eax
	je	.L98
	cmpl	$3, %eax
	je	.L99
	jmp	.L104
.L97:
	movl	-20(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	subl	$1, -16(%ebp)
	subl	$1, -20(%ebp)
.L98:
	movl	-20(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	subl	$1, -16(%ebp)
	subl	$1, -20(%ebp)
.L99:
	movl	-20(%ebp), %eax
	movzbl	(%eax), %edx
	movl	-16(%ebp), %eax
	movb	%dl, (%eax)
	subl	$1, -16(%ebp)
	subl	$1, -20(%ebp)
.L96:
	movl	16(%ebp), %eax
	andl	$3, %eax
	cmpl	$1, %eax
	je	.L101
	cmpl	$1, %eax
	jb	.L100
	cmpl	$2, %eax
	je	.L102
	cmpl	$3, %eax
	je	.L103
	jmp	.L104
.L100:
	movl	-16(%ebp), %edx
	movl	-20(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 455 "string.c" 1
	std
	rep; movsl
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -16(%ebp)
	movl	%edx, -20(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L95
.L101:
	movl	-16(%ebp), %edx
	movl	-20(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 465 "string.c" 1
	std
	rep; movsl
	movsb
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -16(%ebp)
	movl	%edx, -20(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L95
.L102:
	movl	-16(%ebp), %edx
	movl	-20(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 476 "string.c" 1
	std
	rep; movsl
	movsw
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -16(%ebp)
	movl	%edx, -20(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L95
.L103:
	movl	-16(%ebp), %edx
	movl	-20(%ebp), %eax
	movl	16(%ebp), %ecx
	shrl	$2, %ecx
	movl	%edx, %ebx
	movl	%eax, %edx
	movl	%ecx, %eax
	movl	%ebx, %edi
	movl	%edx, %esi
	movl	%eax, %ecx
#APP
# 487 "string.c" 1
	std
	rep; movsl
	movsw
	movsb
	
# 0 "" 2
#NO_APP
	movl	%ecx, %eax
	movl	%esi, %edx
	movl	%edi, %ebx
	movl	%ebx, -16(%ebp)
	movl	%edx, -20(%ebp)
	movl	%eax, 16(%ebp)
	jmp	.L104
.L95:
	jmp	.L104
.L94:
	movl	16(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	memcpy
	jmp	.L93
.L104:
	movl	8(%ebp), %eax
.L93:
	addl	$28, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	memmove, .-memmove
.globl strnlen_user
	.type	strnlen_user, @function
strnlen_user:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	movl	8(%ebp), %eax
	movl	12(%ebp), %edx
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	probe_for_read
	movl	%eax, -12(%ebp)
	cmpl	$0, -12(%ebp)
	jns	.L107
	movl	-12(%ebp), %eax
	jmp	.L108
.L107:
	movl	12(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	strnlen
.L108:
	leave
	ret
	.size	strnlen_user, .-strnlen_user
	.ident	"GCC: (GNU) 4.4.7 20120313 (Red Hat 4.4.7-3)"
	.section	.note.GNU-stack,"",@progbits
