	.file	"vsprintf.c"
	.section	.rodata
	.align 4
.LC0:
	.string	"Assertion failed:\n%s in\nFile:%s Line:%d Function:%s\n"
	.align 4
.LC1:
	.string	"\033[31mKERNEL ERROR\033[0m %s line:%d,kernel bug\n"
	.text
	.type	__assert_fail, @function
__assert_fail:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$56, %esp
	movl	20(%ebp), %eax
	movl	%eax, 16(%esp)
	movl	16(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	12(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	8(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$.LC0, (%esp)
	call	printk
	leal	-16(%ebp), %eax
	movl	%eax, -12(%ebp)
#APP
# 106 "../Include/kernel/kernel.h" 1
	pushf 
pop %edx 
cli
# 0 "" 2
#NO_APP
	movl	-12(%ebp), %eax
	movl	%edx, (%eax)
	movl	curr_log_level, %eax
	cmpl	$4, %eax
	ja	.L2
	movl	$17, 8(%esp)
	movl	$__FUNCTION__.1426, 4(%esp)
	movl	$.LC1, (%esp)
	call	printk
.L2:
	jmp	.L2
	.size	__assert_fail, .-__assert_fail
	.type	number, @function
number:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$116, %esp
	movl	12(%ebp), %eax
	movl	%eax, -112(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, -108(%ebp)
	movl	20(%ebp), %eax
	andl	$16, %eax
	testl	%eax, %eax
	je	.L5
	cmpl	$10, 24(%ebp)
	je	.L5
	movl	$1, %eax
	jmp	.L6
.L5:
	movl	$0, %eax
.L6:
	movl	%eax, -32(%ebp)
	movl	20(%ebp), %eax
	andl	$32, %eax
	movb	%al, -33(%ebp)
	movl	20(%ebp), %eax
	andl	$2, %eax
	testl	%eax, %eax
	je	.L7
	andl	$-9, 20(%ebp)
.L7:
	movb	$0, -34(%ebp)
	movl	20(%ebp), %eax
	andl	$1, %eax
	testb	%al, %al
	je	.L8
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	testl	%edx, %edx
	jns	.L9
	movb	$45, -34(%ebp)
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	negl	%eax
	adcl	$0, %edx
	negl	%edx
	movl	%eax, -112(%ebp)
	movl	%edx, -108(%ebp)
	subl	$1, 28(%ebp)
	jmp	.L8
.L9:
	movl	20(%ebp), %eax
	andl	$64, %eax
	testl	%eax, %eax
	je	.L10
	movb	$43, -34(%ebp)
	subl	$1, 28(%ebp)
	jmp	.L8
.L10:
	movl	20(%ebp), %eax
	andl	$4, %eax
	testl	%eax, %eax
	je	.L8
	movb	$32, -34(%ebp)
	subl	$1, 28(%ebp)
.L8:
	cmpl	$0, -32(%ebp)
	je	.L11
	subl	$1, 28(%ebp)
	cmpl	$16, 24(%ebp)
	jne	.L11
	subl	$1, 28(%ebp)
.L11:
	movl	$0, -28(%ebp)
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	orl	%edx, %eax
	testl	%eax, %eax
	jne	.L12
	movl	-28(%ebp), %eax
	movb	$48, -100(%ebp,%eax)
	addl	$1, -28(%ebp)
	jmp	.L13
.L12:
	cmpl	$10, 24(%ebp)
	je	.L14
	movl	24(%ebp), %eax
	subl	$1, %eax
	movl	%eax, -24(%ebp)
	movl	$3, -20(%ebp)
	cmpl	$16, 24(%ebp)
	jne	.L15
	movl	$4, -20(%ebp)
.L15:
	movl	-28(%ebp), %eax
	movl	-112(%ebp), %edx
	movzbl	%dl, %edx
	andl	-24(%ebp), %edx
	movzbl	digits.1442(%edx), %edx
	orb	-33(%ebp), %dl
	movb	%dl, -100(%ebp,%eax)
	addl	$1, -28(%ebp)
	movl	-20(%ebp), %ecx
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	shrdl	%edx, %eax
	shrl	%cl, %edx
	testb	$32, %cl
	je	.L34
	movl	%edx, %eax
	xorl	%edx, %edx
.L34:
	movl	%eax, -112(%ebp)
	movl	%edx, -108(%ebp)
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	orl	%edx, %eax
	testl	%eax, %eax
	jne	.L15
	jmp	.L13
.L14:
	movl	-28(%ebp), %ecx
	movl	-112(%ebp), %eax
	movl	24(%ebp), %ebx
	movl	$0, %edx
	divl	%ebx
	movl	%edx, %eax
	movl	%eax, -16(%ebp)
	movl	-112(%ebp), %eax
	movl	24(%ebp), %edx
	movl	%edx, -116(%ebp)
	movl	$0, %edx
	divl	-116(%ebp)
	movl	$0, %edx
	movl	%eax, -112(%ebp)
	movl	%edx, -108(%ebp)
	movl	-16(%ebp), %eax
	movzbl	digits.1442(%eax), %eax
	orb	-33(%ebp), %al
	movb	%al, -100(%ebp,%ecx)
	addl	$1, -28(%ebp)
	movl	-112(%ebp), %eax
	movl	-108(%ebp), %edx
	orl	%edx, %eax
	testl	%eax, %eax
	jne	.L14
.L13:
	movl	-28(%ebp), %eax
	cmpl	32(%ebp), %eax
	jle	.L16
	movl	-28(%ebp), %eax
	movl	%eax, 32(%ebp)
.L16:
	movl	32(%ebp), %eax
	subl	%eax, 28(%ebp)
	movl	20(%ebp), %eax
	andl	$10, %eax
	testl	%eax, %eax
	jne	.L17
	jmp	.L18
.L19:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L18:
	subl	$1, 28(%ebp)
	cmpl	$0, 28(%ebp)
	jns	.L19
.L17:
	cmpb	$0, -34(%ebp)
	je	.L20
	movl	8(%ebp), %eax
	movzbl	-34(%ebp), %edx
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
.L20:
	cmpl	$0, -32(%ebp)
	je	.L21
	movl	8(%ebp), %eax
	movb	$48, (%eax)
	addl	$1, 8(%ebp)
	cmpl	$16, 24(%ebp)
	jne	.L21
	movzbl	-33(%ebp), %eax
	movl	%eax, %edx
	orl	$88, %edx
	movl	8(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
.L21:
	movl	20(%ebp), %eax
	andl	$2, %eax
	testl	%eax, %eax
	jne	.L27
	movl	20(%ebp), %eax
	andl	$8, %eax
	testl	%eax, %eax
	je	.L23
	movl	$48, %eax
	jmp	.L24
.L23:
	movl	$32, %eax
.L24:
	movb	%al, -9(%ebp)
	jmp	.L25
.L26:
	movl	8(%ebp), %eax
	movzbl	-9(%ebp), %edx
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
.L25:
	subl	$1, 28(%ebp)
	cmpl	$0, 28(%ebp)
	jns	.L26
	jmp	.L27
.L28:
	movl	8(%ebp), %eax
	movb	$48, (%eax)
	addl	$1, 8(%ebp)
.L27:
	subl	$1, 32(%ebp)
	movl	32(%ebp), %eax
	cmpl	-28(%ebp), %eax
	jge	.L28
	jmp	.L29
.L30:
	movl	-28(%ebp), %eax
	movzbl	-100(%ebp,%eax), %edx
	movl	8(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
.L29:
	subl	$1, -28(%ebp)
	cmpl	$0, -28(%ebp)
	jns	.L30
	jmp	.L31
.L32:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L31:
	cmpl	$0, 28(%ebp)
	jns	.L32
	movl	8(%ebp), %eax
	addl	$116, %esp
	popl	%ebx
	popl	%ebp
	ret
	.size	number, .-number
	.section	.rodata
.LC2:
	.string	"(null)"
	.text
.globl vsnprintf
	.type	vsnprintf, @function
vsnprintf:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$104, %esp
	movl	12(%ebp), %eax
	movl	%eax, -68(%ebp)
	movl	$0, -52(%ebp)
	movl	$0, -48(%ebp)
	movl	$0, -44(%ebp)
	movl	$0, -40(%ebp)
	movl	$10, -36(%ebp)
	movl	8(%ebp), %eax
	movl	%eax, -32(%ebp)
	jmp	.L36
.L112:
	movl	16(%ebp), %eax
	movzbl	(%eax), %eax
	cmpb	$37, %al
	je	.L37
	movl	16(%ebp), %eax
	movzbl	(%eax), %edx
	movl	8(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
	addl	$1, 16(%ebp)
	subl	$1, -68(%ebp)
	jmp	.L36
.L37:
	movl	$0, -52(%ebp)
	movb	$0, -25(%ebp)
	movl	$10, -36(%ebp)
	movl	$0, -40(%ebp)
	movl	$0, -44(%ebp)
	addl	$1, 16(%ebp)
.L38:
	movl	16(%ebp), %eax
	movzbl	(%eax), %eax
	movb	%al, -26(%ebp)
	movsbl	-26(%ebp), %eax
	addl	$1, 16(%ebp)
	subl	$32, %eax
	cmpl	$90, %eax
	ja	.L36
	movl	.L63(,%eax,4), %eax
	jmp	*%eax
	.section	.rodata
	.align 4
	.align 4
.L63:
	.long	.L39
	.long	.L36
	.long	.L36
	.long	.L40
	.long	.L36
	.long	.L41
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L42
	.long	.L43
	.long	.L36
	.long	.L44
	.long	.L45
	.long	.L36
	.long	.L46
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L47
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L48
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L49
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L50
	.long	.L36
	.long	.L51
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L52
	.long	.L53
	.long	.L36
	.long	.L36
	.long	.L36
	.long	.L54
	.long	.L53
	.long	.L36
	.long	.L36
	.long	.L55
	.long	.L36
	.long	.L36
	.long	.L56
	.long	.L57
	.long	.L36
	.long	.L36
	.long	.L58
	.long	.L59
	.long	.L60
	.long	.L36
	.long	.L36
	.long	.L61
	.long	.L36
	.long	.L62
	.text
.L40:
	orl	$16, -52(%ebp)
	jmp	.L38
.L43:
	orl	$64, -52(%ebp)
	jmp	.L38
.L44:
	orl	$2, -52(%ebp)
	jmp	.L38
.L39:
	orl	$4, -52(%ebp)
	jmp	.L38
.L46:
	orl	$8, -52(%ebp)
	jmp	.L38
.L41:
	movl	8(%ebp), %eax
	movb	$37, (%eax)
	addl	$1, 8(%ebp)
	subl	$1, -68(%ebp)
	jmp	.L36
.L47:
	movl	$0, -24(%ebp)
.L65:
	movl	-24(%ebp), %edx
	movl	%edx, %eax
	sall	$2, %eax
	addl	%edx, %eax
	addl	%eax, %eax
	movl	%eax, %edx
	movsbl	-26(%ebp), %eax
	leal	(%edx,%eax), %eax
	subl	$48, %eax
	movl	%eax, -24(%ebp)
	movl	16(%ebp), %eax
	movzbl	(%eax), %eax
	movb	%al, -26(%ebp)
	cmpb	$47, -26(%ebp)
	jle	.L64
	cmpb	$57, -26(%ebp)
	jg	.L64
	addl	$1, 16(%ebp)
	jmp	.L65
.L64:
	cmpl	$0, -40(%ebp)
	je	.L66
	movl	-24(%ebp), %eax
	movl	%eax, -44(%ebp)
	jmp	.L38
.L66:
	movl	-24(%ebp), %eax
	movl	%eax, -48(%ebp)
	jmp	.L38
.L42:
	cmpl	$0, -40(%ebp)
	je	.L68
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, -44(%ebp)
	jmp	.L69
.L68:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, -48(%ebp)
.L69:
	subl	$1, -68(%ebp)
	jmp	.L36
.L45:
	movl	$1, -40(%ebp)
	jmp	.L38
.L52:
	movl	-52(%ebp), %eax
	andl	$2, %eax
	testl	%eax, %eax
	jne	.L70
	jmp	.L71
.L72:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L71:
	subl	$1, -48(%ebp)
	cmpl	$0, -48(%ebp)
	jg	.L72
.L70:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movb	%al, -17(%ebp)
	movl	8(%ebp), %eax
	movzbl	-17(%ebp), %edx
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
	subl	$1, -68(%ebp)
	jmp	.L73
.L74:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L73:
	subl	$1, -48(%ebp)
	cmpl	$0, -48(%ebp)
	jg	.L74
	jmp	.L36
.L58:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, -16(%ebp)
	cmpl	$0, -16(%ebp)
	jne	.L75
	movl	$.LC2, -16(%ebp)
.L75:
	cmpl	$0, -40(%ebp)
	jne	.L76
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	strlen
	movl	%eax, -12(%ebp)
	jmp	.L77
.L76:
	movl	$0, -12(%ebp)
	jmp	.L78
.L79:
	addl	$1, -12(%ebp)
.L78:
	movl	-12(%ebp), %eax
	cmpl	-44(%ebp), %eax
	jge	.L77
	movl	-12(%ebp), %eax
	addl	-16(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	jne	.L79
.L77:
	movl	-12(%ebp), %eax
	subl	%eax, -48(%ebp)
	movl	-52(%ebp), %eax
	andl	$2, %eax
	testl	%eax, %eax
	jne	.L115
	cmpl	$0, -48(%ebp)
	jle	.L116
	jmp	.L81
.L82:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L81:
	cmpl	$0, -48(%ebp)
	setg	%al
	subl	$1, -48(%ebp)
	testb	%al, %al
	jne	.L82
	jmp	.L83
.L84:
	movl	-16(%ebp), %eax
	movzbl	(%eax), %edx
	movl	8(%ebp), %eax
	movb	%dl, (%eax)
	addl	$1, 8(%ebp)
	addl	$1, -16(%ebp)
	subl	$1, -68(%ebp)
	jmp	.L83
.L115:
	nop
	jmp	.L83
.L116:
	nop
.L83:
	cmpl	$0, -12(%ebp)
	setg	%al
	subl	$1, -12(%ebp)
	testb	%al, %al
	jne	.L84
	movl	-52(%ebp), %eax
	andl	$2, %eax
	testl	%eax, %eax
	je	.L117
	cmpl	$0, -48(%ebp)
	jle	.L118
	jmp	.L86
.L87:
	movl	8(%ebp), %eax
	movb	$32, (%eax)
	addl	$1, 8(%ebp)
.L86:
	cmpl	$0, -48(%ebp)
	setg	%al
	subl	$1, -48(%ebp)
	testb	%al, %al
	jne	.L87
	jmp	.L36
.L53:
	orl	$1, -52(%ebp)
	movl	$10, -36(%ebp)
	nop
.L88:
	movsbl	-25(%ebp), %eax
	subl	$72, %eax
	cmpl	$50, %eax
	ja	.L103
	jmp	.L114
.L49:
	movb	$76, -25(%ebp)
	jmp	.L38
.L48:
	movb	$72, -25(%ebp)
	jmp	.L38
.L55:
	cmpb	$108, -25(%ebp)
	jne	.L89
	movb	$76, -25(%ebp)
	jmp	.L38
.L89:
	movb	$108, -25(%ebp)
	jmp	.L38
.L62:
	movb	$122, -25(%ebp)
	jmp	.L38
.L51:
	movb	$122, -25(%ebp)
	jmp	.L38
.L54:
	cmpb	$104, -25(%ebp)
	jne	.L91
	movb	$72, -25(%ebp)
	jmp	.L38
.L91:
	movb	$104, -25(%ebp)
	jmp	.L38
.L59:
	movb	$116, -25(%ebp)
	jmp	.L38
.L57:
	movl	$16, -36(%ebp)
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L56:
	movl	$8, -36(%ebp)
	jmp	.L94
.L60:
	movl	$10, -36(%ebp)
	jmp	.L94
.L61:
	orl	$32, -52(%ebp)
	movl	$16, -36(%ebp)
	jmp	.L94
.L50:
	movl	$16, -36(%ebp)
.L94:
	movsbl	-25(%ebp), %eax
	subl	$72, %eax
	cmpl	$50, %eax
	ja	.L95
	movl	.L102(,%eax,4), %eax
	jmp	*%eax
	.section	.rodata
	.align 4
	.align 4
.L102:
	.long	.L96
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L97
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L98
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L99
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L100
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L95
	.long	.L101
	.text
.L97:
	movl	20(%ebp), %eax
	leal	8(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	4(%eax), %edx
	movl	(%eax), %eax
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L99:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L98:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movzwl	%ax, %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L96:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movzbl	%al, %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L101:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L100:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L95:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	$0, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L114:
	movl	.L110(,%eax,4), %eax
	jmp	*%eax
	.section	.rodata
	.align 4
	.align 4
.L110:
	.long	.L104
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L105
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L106
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L107
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L108
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L103
	.long	.L109
	.text
.L105:
	movl	20(%ebp), %eax
	leal	8(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	4(%eax), %edx
	movl	(%eax), %eax
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L107:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L106:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	cwtl
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L104:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movsbl	%al, %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L109:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L108:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
	jmp	.L93
.L103:
	movl	20(%ebp), %eax
	leal	4(%eax), %edx
	movl	%edx, 20(%ebp)
	movl	(%eax), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, -64(%ebp)
	movl	%edx, -60(%ebp)
.L93:
	movl	-64(%ebp), %eax
	movl	-60(%ebp), %edx
	movl	-44(%ebp), %ecx
	movl	%ecx, 24(%esp)
	movl	-48(%ebp), %ecx
	movl	%ecx, 20(%esp)
	movl	-36(%ebp), %ecx
	movl	%ecx, 16(%esp)
	movl	-52(%ebp), %ecx
	movl	%ecx, 12(%esp)
	movl	%eax, 4(%esp)
	movl	%edx, 8(%esp)
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	number
	movl	%eax, 8(%ebp)
	jmp	.L36
.L117:
	nop
	jmp	.L36
.L118:
	nop
.L36:
	movl	16(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	je	.L111
	cmpl	$0, -68(%ebp)
	jne	.L112
.L111:
	movl	8(%ebp), %eax
	movb	$0, (%eax)
	movl	8(%ebp), %edx
	movl	-32(%ebp), %eax
	movl	%edx, %ecx
	subl	%eax, %ecx
	movl	%ecx, %eax
	leave
	ret
	.size	vsnprintf, .-vsnprintf
.globl snprintf
	.type	snprintf, @function
snprintf:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	leal	20(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	16(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	vsnprintf
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	leave
	ret
	.size	snprintf, .-snprintf
.globl sprintf
	.type	sprintf, @function
sprintf:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	leal	16(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	12(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	$-1, 4(%esp)
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	vsnprintf
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	leave
	ret
	.size	sprintf, .-sprintf
	.section	.rodata
.LC3:
	.string	"vsprintf.c"
.LC4:
	.string	"buf != ((void *)0)"
	.text
.globl printk
	.type	printk, @function
printk:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	movl	$256, (%esp)
	call	kzalloc
	movl	%eax, -12(%ebp)
	cmpl	$0, -12(%ebp)
	jne	.L124
	movl	$__PRETTY_FUNCTION__.1788, 12(%esp)
	movl	$396, 8(%esp)
	movl	$.LC3, 4(%esp)
	movl	$.LC4, (%esp)
	call	__assert_fail
.L124:
	leal	12(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	-20(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	8(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	$-1, 4(%esp)
	movl	-12(%ebp), %eax
	movl	%eax, (%esp)
	call	vsnprintf
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	-12(%ebp), %eax
	movl	%eax, (%esp)
	call	write_string
	movl	-12(%ebp), %eax
	movl	%eax, (%esp)
	call	kfree
	movl	-16(%ebp), %eax
	leave
	ret
	.size	printk, .-printk
	.section	.rodata
	.type	__PRETTY_FUNCTION__.1788, @object
	.size	__PRETTY_FUNCTION__.1788, 7
__PRETTY_FUNCTION__.1788:
	.string	"printk"
	.type	digits.1442, @object
	.size	digits.1442, 16
digits.1442:
	.ascii	"0123456789ABCDEF"
	.type	__FUNCTION__.1426, @object
	.size	__FUNCTION__.1426, 14
__FUNCTION__.1426:
	.string	"__assert_fail"
	.ident	"GCC: (GNU) 4.4.7 20120313 (Red Hat 4.4.7-3)"
	.section	.note.GNU-stack,"",@progbits
