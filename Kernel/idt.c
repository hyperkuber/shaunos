/*
 * idt.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/kernel.h>
#include <kernel/idt.h>
#include <kernel/cpu.h>


extern void _isr_divide_error(void);
extern void _isr_debug(void);
extern void _isr_nmi(void);
extern void _isr_int3(void);
extern void _isr_overflow(void);
extern void _isr_bounds(void);
extern void _isr_invalid_op(void);
extern void _isr_device_not_available(void);
extern void _isr_double_fault(void);
extern void _isr_coprocessor_segment_overrun(void);
extern void _isr_invalid_tss(void);
extern void _isr_segment_not_present(void);
extern void _isr_stack_segment(void);
extern void _isr_general_protection(void);
extern void _isr_page_fault(void);
extern void _isr_coprocessor_error(void);
extern void _isr_reserved(void);
extern void _isr_syscall(void);
extern void _isr_mechine_check(void);
extern void _isr_align_check(void);
extern void _isr_simd_error(void);
extern void _isr_fpu(void);

extern void _irq0(void);
extern void _irq1(void);
extern void _irq2(void);
extern void _irq3(void);
extern void _irq4(void);
extern void _irq5(void);
extern void _irq6(void);
extern void _irq7(void);
extern void _irq8(void);
extern void _irq9(void);
extern void _irqa(void);
extern void _irqb(void);
extern void _irqc(void);
extern void _irqd(void);
extern void _irqe(void);
extern void _irqf(void);




struct idt_desc idt_entries[NR_IDT_ENTRIES];

void set_trap_gate(int vector, unsigned int handler, int dpl)
{

	idt_entries[vector].addlow = handler & 0xFFFF;
	idt_entries[vector].addrhigh =(handler & 0xFFFF0000) >> 16;
	idt_entries[vector].dpl = dpl;
	idt_entries[vector].present = 0x1;
	idt_entries[vector].reserved = 0x0;
	idt_entries[vector].segselector = KERNEL_CS;
	idt_entries[vector].type = 0x0F;

}

void set_interupt_gate(int vector, unsigned int handler, int dpl)
{

	idt_entries[vector].addlow = handler & 0xFFFF;
	idt_entries[vector].addrhigh = (handler & 0xFFFF0000) >> 16;
	idt_entries[vector].dpl = dpl;
	idt_entries[vector].present = 0x1;
	idt_entries[vector].reserved = 0x0;
	idt_entries[vector].segselector = KERNEL_CS;
	idt_entries[vector].type = 0x0E;
}

void idt_init(void)
{
	int i;
	struct idt_ptr idtr;

	for (i=0; i<NR_IDT_ENTRIES; i++){
		set_trap_gate(i, (uint_t)_isr_reserved, 0);
	}

	set_interupt_gate(0, (uint_t)_isr_divide_error, 0);
	set_interupt_gate(1, (uint_t)_isr_debug, 0);
	set_interupt_gate(2, (uint_t)_isr_nmi, 0);
	set_interupt_gate(3, (uint_t)_isr_int3, 3);
	set_interupt_gate(4, (uint_t)_isr_overflow, 3);
	set_interupt_gate(5, (uint_t)_isr_bounds, 3);
	set_interupt_gate(6, (uint_t)_isr_invalid_op, 0);
	set_interupt_gate(7, (uint_t)_isr_device_not_available, 0);
	set_interupt_gate(8, (uint_t)_isr_double_fault, 0);
	set_interupt_gate(9, (uint_t)_isr_coprocessor_segment_overrun, 0);
	set_interupt_gate(10, (uint_t)_isr_invalid_tss, 0);
	set_interupt_gate(11, (uint_t)_isr_segment_not_present, 0);
	set_interupt_gate(12, (uint_t)_isr_stack_segment, 0);
	set_interupt_gate(13, (uint_t)_isr_general_protection, 0);
	set_interupt_gate(14, (uint_t)_isr_page_fault, 0);
	//set_trap_gate(15, (uint_t)_isr_reserved, 0);	//
	set_interupt_gate(16, (uint_t)_isr_fpu, 0);
	set_interupt_gate(17, (uint_t)_isr_align_check, 0);
	set_interupt_gate(18, (uint_t)_isr_mechine_check, 0);
	set_interupt_gate(19, (uint_t)_isr_simd_error, 0);

	set_interupt_gate(0x20, (uint_t)_irq0, 0);
	set_interupt_gate(0x21, (uint_t)_irq1, 0);
	set_interupt_gate(0x22, (uint_t)_irq2, 0);
	set_interupt_gate(0x23, (uint_t)_irq3, 0);
	set_interupt_gate(0x24, (uint_t)_irq4, 0);
	set_interupt_gate(0x25, (uint_t)_irq5, 0);
	set_interupt_gate(0x26, (uint_t)_irq6, 0);
	set_interupt_gate(0x27, (uint_t)_irq7, 0);
	set_interupt_gate(0x28, (uint_t)_irq8, 0);	//pic slave start
	set_interupt_gate(0x29, (uint_t)_irq9, 0);
	set_interupt_gate(0x2a, (uint_t)_irqa, 0);
	set_interupt_gate(0x2b, (uint_t)_irqb, 0);
	set_interupt_gate(0x2c, (uint_t)_irqc, 0);
	set_interupt_gate(0x2d, (uint_t)_irqd, 0);
	set_interupt_gate(0x2e, (uint_t)_irqe, 0);
	set_interupt_gate(0x2f, (uint_t)_irqf, 0);


	set_trap_gate(SYSCALL,	(uint_t)_isr_syscall, 3);

	idtr.limit = sizeof(idt_entries);
	idtr.base = (u32)&idt_entries;

	  __asm__ __volatile__ ("lidt (%0)\n\t"
				::"p" (&idtr));
}


//void do_divide_error(struct interrupt_state *state)
//{
//	printk("do_divide_error:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_debug(struct interrupt_state *state)
//{
//	printk("do_debug:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//
//}
//void do_nmi(struct interrupt_state *state)
//{
//	printk("do_nmi:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_int3(struct interrupt_state *state)
//{
//	printk("do_int3:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//
//}
//void do_overflow(struct interrupt_state *state)
//{
//	printk("do_overflow:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_bounds(struct interrupt_state *state)
//{
//	printk("do_bounds:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_invalid_op(struct interrupt_state *state)
//{
//	printk("do_invalid_op:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//
//}
//void do_device_not_available(struct interrupt_state *state)
//{
//	printk("do_device_not_available:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_double_fault(struct interrupt_state *state)
//{
//	printk("do_double_fault:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_coprocessor_segment_overrun(struct interrupt_state *state)
//{
//	printk("do_coprocessor_segment_overrun:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_invalid_TSS(struct interrupt_state *state)
//{
//	printk("do_invalid_TSS:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_segment_not_present(struct interrupt_state *state)
//{
//	printk("do_segment_not_present:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//
//}
//
//void do_stack_segment(struct interrupt_state *state)
//{
//	printk("do_stack_segment:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//}
//void do_general_protection(struct interrupt_state *state)
//{
//	printk("do_general_protection:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1);
//
//}
//void do_page_fault(struct interrupt_state *state)
//{
//	unsigned long error_vaddr = get_cr2();
//	LOG_INFO("page fault, vaddr:%x, error:%x", error_vaddr, state->error_code);
//
//	while (1){};
//}
//
//void do_coprocessor_error(struct interrupt_state *state)
//{
//	printk("do_coprocessor_error:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d\n", state->error_code,
//			state->fs, (ushort_t)state->ds, state->es,state->cs);
//	while (1){};
//}
//void do_reserved(struct interrupt_state *state)
//{
//	//in P6 CPU,it is a bug that when local APIC interrupt happens,
//	//the cpu also generates a reserved interrupt vector, and the iret/iretd
//	//will also pop that interrupt vector out during context switching,
//	//so i just comment the log below ...
////	printk("do_reserved:error_code:%d, fs:%d, ds:%d, es:%d, cs:%d, eip:%d\n", state->error_code,
////			state->fs, (ushort_t)state->ds, state->es,state->cs, state->eip);
//	//while (1){};
//}
