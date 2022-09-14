/*
 * main.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/console.h>
#include <string.h>
#include <asm/system.h>
#include <kernel/segment.h>
#include <kernel/kernel.h>
#include <kernel/mm.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/mouse.h>
#include <kernel/tss.h>
#include <kernel/kthread.h>
#include <kernel/ide.h>
#include <kernel/page.h>
#include <kernel/multiboot.h>
#include <kernel/io_dev.h>
#include <kernel/ext2.h>
#include <list.h>
#include <kernel/vfs.h>
#include <kernel/bitops.h>
#include <kernel/time.h>
#include <kernel/pid.h>
#include <kernel/int86.h>
#include <kernel/irq.h>
#include <kernel/pci.h>
#include <kernel/ahci.h>
#include <kernel/malloc.h>
#include <kernel/net/netisr.h>
#include <kernel/sys/domain.h>
#include <kernel/sys/netinet/dhcp.h>
#include <kernel/net/if_ether.h>
#include <kernel/gfx/gfx.h>
#define _test_net

#ifdef _test_net
#include <kernel/sys/socket.h>
#include <kernel/sys/netinet/in.h>
#include <kernel/sys/netinet/in_val.h>
#include <kernel/sys/socketval.h>
#include <kernel/mbuf.h>

#endif

ssize_t read(int fd, void *buf, ssize_t count);
void setattr(unsigned char attr);
void * init(void *args );
kthread_u_t *gfx_isr_thread;
extern kthread_u_t *sysidle_thread;
extern kthread_u_t *io_scheduler;
extern unsigned long pg0[];
extern int video_init();

u32 curr_log_level;
u32 gfxisr_mask;
u32 graphic = 1;

int kmain(unsigned long magic, unsigned long addr)
{
	multiboot_info_t *mbi = (multiboot_info_t *) addr;
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC){
		return -1;
	}
	if (mbi->flags & MULTIBOOT_INFO_VIDEO_INFO){
		//printk("video info\n");
	}
	if (mbi->flags & MULTIBOOT_INFO_DRIVE_INFO){
		//printk("dirve info addr:%x", mbi->drives_addr);
	}
	console_init();
	//kernel heap is 2m bytes,including network buffer(1m)
	heap_init((u32)pg0 + 4096, 0x200000);
	paging_init(mbi);
	init_gdt();
	idt_init();
	tss_init();
	irq_init();
	int86_init();
	mem_init(mbi);
	keyboard_init();
	mouse_init();
	timer_init();
	time_init();
	pci_init();

	schedule_init();
	io_scheduler = kcreate_thread((unsigned long)io_schedule, 0, NULL, NULL);
	netisr_thread = kcreate_thread((unsigned long )netisr, 0, NULL, NULL);
	timer_isr_thread = kcreate_thread((u32)timerisr, 0, NULL, NULL);
	gfx_isr_thread = kcreate_thread((unsigned long)init, 0, NULL, NULL);
	asm volatile (
			"movl	%0,	%%esp\n\t"
			"popl	%%gs\n\t"
			"popl	%%fs\n\t"
			"popl	%%ds\n\t"
			"popl	%%es\n\t"
			"popl	%%ebp\n\t"
			"popl	%%esi\n\t"
			"popl	%%edi\n\t"
			"popl	%%edx\n\t"
			"popl	%%ecx\n\t"
			"popl	%%ebx\n\t"
			"popl	%%eax\n\t"
			"iret\n\t"
			:
			:"r"(sysidle_thread->kthread.esp));

	return 0;
}

/*
 * the second startup phase, in
 * this phase, the scheduler and timer should
 * work fine.
 */
void *init(void *args)
{
	int retval,x;
	struct mouse_input val;
	struct kbd_input key;
	kthread_att_t att;
	wait_queue_head_t wq;
	att.priority = 0;
	att.priv = 3;
	att.state = RUNNING;
	int proc_id;
	dev_t *pmoused;
	dev_t *pkbd;

	if (ahci_init() <0){
		LOG_INFO("ahci controller init failed");
		while (1);
	}


	//ide_initialize();
	console_fs_init();
	if (ext2_fs_init() < 0) {
		LOG_ERROR("mount ext2 fs failed");
		BUG();
	}

	leinit();
	ifinit();
	domaininit();
	dhcp_get_ip();


	list_init(&wq.waiter);
	if (open_device("PS/2 mouse", &pmoused) < 0) {
		LOG_ERROR("open mouse dev failed");
		BUG();
	}
	if (open_device("keyboard", &pkbd) < 0) {
		LOG_ERROR("open keyboard failed");
		BUG();
	}

	if (graphic) {
		video_init();

		char *argv[] = {"test","argv2", 0};
		char *envp[] = {"PATH=/",0};
		proc_id = kcreate_process("/bin/dsktp", argv, envp, &att, 0);
		if (proc_id < 0){
			LOG_ERROR("create process error");
			return NULL;
		}

		while (graphic) {
			local_irq_save(&x);
			list_del(&current->thread_list);
			list_add_tail(&wq.waiter, &current->thread_list);
	//		kbd_add_waiter(pkbd, &wq);
	//		ms_add_waiter(pmoused, &wq);
			gfxisr_mask &= ~GFXISR_ACT;
			local_irq_restore(x);

			retval = 0;

			schedule();

			gfxisr_mask |= GFXISR_ACT;

			if (gfxisr_mask & GFXISR_MOUSE) {
				do {
					memset((void *)&val, 0, sizeof(val));
					retval = pmoused->ops->read(pmoused, (void *)&val, sizeof(val));
					if (retval > 0) {
						//LOG_INFO("mouse val.dx:%d dy:%d",  val.ms_dx, val.ms_dy);
						local_irq_save(&x);
						frame_mouse_event(&val);
						local_irq_restore(x);
					}
				} while (retval > 0);
				gfxisr_mask &= ~GFXISR_MOUSE;
			}

			if (gfxisr_mask & GFXISR_KBD) {
				do {
					retval = pkbd->ops->read(pkbd, &key, sizeof(key));
					if (retval > 0) {
						frame_kbd_event(&key);
					}
				} while (retval > 0);
				gfxisr_mask &= ~GFXISR_KBD;
			}
		}
	}

	char *argv[] = {"test","argv2", 0};
	char *envp[] = {"PATH=/",0};
	proc_id = kcreate_process("/bin/init", argv, envp, &att, 0);
	if (proc_id < 0){
		LOG_ERROR("create process error");
		return NULL;
	}




	curr_log_level = KLOG_LEVEL_INFO;

	retval = kwait_pid(proc_id, WAITE_INFINITE);

	switch (retval){
	case WAITE_OK:
		LOG_INFO("wait ok");
		break;
	case WAITE_TIMEOUT:
		LOG_INFO("wait timeout");
		break;
	default:
		LOG_INFO("wait error");
		break;
	}
	return NULL;
}



