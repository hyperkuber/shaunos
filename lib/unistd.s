	.file	"unistd.c"
	.text
.globl exit
	.type	exit, @function
exit:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
.L2:
	movl	$0, %eax
	movl	8(%ebp), %edx
	movl	%edx, %ebx
#APP
# 14 "unistd.c" 1
	int $0x80
	
# 0 "" 2
#NO_APP
	jmp	.L2
	.size	exit, .-exit
.globl open
	.type	open, @function
open:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$16, %esp
	movl	$1, %eax
	movl	8(%ebp), %ebx
	movl	12(%ebp), %ecx
	movl	16(%ebp), %edx
#APP
# 23 "unistd.c" 1
	int $0x80
	
# 0 "" 2
#NO_APP
	movl	%eax, -8(%ebp)
	movl	-8(%ebp), %eax
	addl	$16, %esp
	popl	%ebx
	popl	%ebp
	ret
	.size	open, .-open
.globl write
	.type	write, @function
write:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$16, %esp
	movl	12(%ebp), %ecx
	movl	$3, %eax
	movl	8(%ebp), %ebx
	movl	16(%ebp), %edx
#APP
# 32 "unistd.c" 1
	int $0x80
	
# 0 "" 2
#NO_APP
	movl	%eax, -8(%ebp)
	movl	-8(%ebp), %eax
	addl	$16, %esp
	popl	%ebx
	popl	%ebp
	ret
	.size	write, .-write
.globl sleep
	.type	sleep, @function
sleep:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$16, %esp
	movl	$6, %eax
	movl	8(%ebp), %edx
	movl	%edx, %ebx
#APP
# 42 "unistd.c" 1
	int $0x80
	
# 0 "" 2
#NO_APP
	movl	%eax, -8(%ebp)
	movl	-8(%ebp), %eax
	addl	$16, %esp
	popl	%ebx
	popl	%ebp
	ret
	.size	sleep, .-sleep
	.ident	"GCC: (GNU) 4.4.7 20120313 (Red Hat 4.4.7-3)"
	.section	.note.GNU-stack,"",@progbits
