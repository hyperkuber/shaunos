/*
 * kthread.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef KTHREAD_H_
#define KTHREAD_H_


#include <kernel/types.h>
#include <kernel/segment.h>
#include <list.h>
#include <kernel/vfs.h>
#include <kernel/page.h>
#include <kernel/pid.h>
#include <kernel/avltree.h>
#include <asm/io.h>
#include <kernel/resource.h>
//#include <kernel/gfx/gfx.h>


#define THREAD_SIZE	(8192)
#define MAX_OPENFILES	(PAGE_SIZE / (sizeof(unsigned long)))
#define PROC_NAME_MAX	32

#define KTHREAD_PROMPT (1<<0)


struct stack_frame {
	u32	gs;
	u32	fs;
	u32	ds;
	u32	es;
	u32	ebp;
	u32	edi;
	u32	esi;
	u32	edx;
	u32	ecx;
	u32	ebx;
	u32	eax;
	u32	eip;
	u32	cs;
	u32	eflags;
	u32	esp;
	u32	ss;
};



struct frame;

struct kernel_process;
typedef struct kernel_thread {
	u32 esp;
	u32 eip;
	u32 ss;
	u32 esp0;
	u32 priv;
	u32 flag;
	u32 ktime;
	u32 utime;
	struct kernel_process *host;
	void *ustack;
	void *kstack;
	ulong_t ticks;
	u32 state;
	u32 tid;
	u32 pid;
	u32 priority;	//for real time schedule
	struct linked_list thread_list;
	wait_queue_head_t wq;
	char *wchan;
	struct frame *active_frame;
} packed kthread_t;


typedef union kthread_u {
	kthread_t kthread;
	u32 kstack[THREAD_SIZE/sizeof(u32)];
}packed kthread_u_t;



typedef struct _image {
	u32 code_start;
	u32 code_end;
	u32 data_satrt;
	u32 data_end;
	u32 bss_start;
	u32 bss_end;
	u32 stack_start;
	u32 brk;
	u32 env_start;
	u32 env_end;
	u32 arg_start;
	u32 arg_end;
}packed image_t;


typedef struct _vm {
	u32 base;
	u32 len;
	u32 flag;
} packed vm_t;

typedef struct _mm {
	struct _image image;
	u32 shared_start;
	u32 shared_end;
	vm_t *vm_page;
	u32 vm_max;
}packed mm_t;

#define VM_PROC_SHIFT	1
#define VM_PGD_SHIFT	2
#define VM_FILE_SHIFT	3
#define VM_RSVD_SHIFT	4
#define VM_PGT_SHIT		5
#define VM_SHARED_SHIFT	6
#define VM_THREADU_SHIFT	7
#define VM_STACK_SHIFT	8
#define VM_ENV_SHIFT	9
#define VM_COMMON_SHIFT	10
#define VM_FRAME_SHIFT	11

#define VM_PROC			(1UL << VM_PROC_SHIFT)
#define VM_PGD			(1UL << VM_PGD_SHIFT)
#define VM_FILE			(1UL << VM_FILE_SHIFT)
#define VM_RSVD			(1UL << VM_RSVD_SHIFT)
#define VM_PGT			(1UL << VM_PGT_SHIT)
#define VM_SHARED		(1UL << VM_SHARED_SHIFT)
#define VM_THREADU		(1Ul << VM_THREADU_SHIFT)
#define VM_STACK		(1UL << VM_STACK_SHIFT)
#define VM_ENV			(1UL << VM_ENV_SHIFT)
#define VM_COMMON		(1UL << VM_COMMON_SHIFT)
#define VM_FRAME		(1UL << VM_FRAME_SHIFT)

#define WAITE_INFINITE	(-1)
#define WAITE_OK		(0)
#define WAITE_TIMEOUT	(1)

struct frame_msg;

typedef struct kernel_process {
	s8 p_name[PROC_NAME_MAX];
	u32 **file;
	u32 **frame;
	vnode_t *p_currdir;
	u32 pid;
	u32 cr3;
	u32 ppid;
	u32	uid;
	u32 gid;
	u32 egid;
	u32 euid;
	s32 open_files;
	u32 p_status;
	u32 p_flags;
	s32	nice;
	u32 p_nrmsgs;
	file_t *p_old_cwd;
	file_t *p_curr_cwd;
	//struct segment_descriptor ldts[2];
	struct linked_list p_proc_list;
	struct kid_object p_tid_obj;
	struct kid_object p_fd_obj;
	struct kid_object p_fmid_obj;
	struct _mm mm;
	struct avlnode *p_threadtree;
	struct kernel_process *p_parent;
	struct kernel_process *p_child;
	wait_queue_head_t p_wq;
	struct linked_list p_silbing;
	wait_queue_head_t *p_w4q;
	struct frame_msg *p_msgq;
	struct frame *p_active_frame;
	struct elf_module *ems;
	struct rusage rs;
	struct rlimit lmt[RLIM_NLIMITS];
} packed kprocess_t, kproc_t;


typedef enum {
	RUNNING = 0,
	SLEEPING,
	STOPED,
	SUSPEND,
} kthread_state_t;


typedef struct thread_attribute {
	s32 priv;
	s32 priority;
	kthread_state_t state;
} kthread_att_t;

#define RPL_USER	3
#define RPL_KERN	0
#define MAX_PRIORITY	5

/*
 * process flags
 */
#define PROC_NOTTY	(1<<0)
#define PROC_NOFRMS	(1<<2)


#define PROC_SERVICE	(PROC_NOTTY | PROC_NOFRMS)

typedef void (*thread_func)(ulong_t arg);


static inline kthread_t *get_current_thread()
{
	kthread_t *thread;
	__asm__("andl %%esp,%0; ":"=r" (thread) : "0" (~(THREAD_SIZE - 1)));
	return thread;
}

static inline kprocess_t *get_current_process()
{
	kthread_t *thread = get_current_thread();
	return thread->host;
}
extern kthread_u_t *gfx_isr_thread;

#define current get_current_thread()

struct linked_list run_queue[MAX_PRIORITY];
struct linked_list wait_queue[MAX_PRIORITY];

extern void schedule_init(void);

extern void schedule(void);

extern kthread_u_t * kcreate_thread(u32 entry,
		u32 arg, kprocess_t *host, kthread_att_t *thd_att);
extern int kcreate_process(char *path, char *argv[], char **env, kthread_att_t *att, u32 flag);

extern void vm_insert(kprocess_t *proc, unsigned long base, unsigned long len, unsigned long flag);
extern void kexit(int status);
extern int kwait_pid(int pid, int ms);
extern int kwait(int *status);
extern int ksleep(int ms);
extern int ksbrk(int incr);
extern int kexecve(char *filename, char **argv, char **env);
extern int kisatty(int fd);
extern int kgetpid(void);
extern int kterminate_proc(int pid, int status);
extern int kunlink(char *path);
extern int copy_to_user(char *dest, char *src, int size);
extern int copy_from_user(char *dest, char *src, int size);
extern void wakeup_waiters(wait_queue_head_t *head);
extern void pt_wakeup(struct kernel_thread *kthread);
extern int fd_attach(int fd, kprocess_t *proc, file_t *file);
extern int fd_detach(int fd, kprocess_t *proc);
extern int falloc(kprocess_t *proc, file_t **pfp, int *pfd);
extern int ffree(kprocess_t *proc, file_t *fp, int fd);
extern int kgetrusage(int who, struct rusage *r_usage);
extern int kgetrlimit(int resource, struct rlimit *r_limt);
extern int ksetrlimit(int resource, struct rlimit *r_limt);
extern int probe_for_read(unsigned long addr, int size);
extern int probe_for_write(unsigned long addr, int size);
extern int copy_to_proc(kprocess_t *proc, char *dest, char *src, int size);
extern int copy_from_proc(kprocess_t *proc, char *dest, char *src, int size);
extern void pt_timer_callback(void *data);
static void inline pt_wait_on(wait_queue_head_t *wq){
	int x;
	local_irq_save(&x);
	current->state = SLEEPING;
	list_del(&current->thread_list);
	list_add_tail(&wq->waiter, &current->thread_list);
	schedule();
	local_irq_restore(x);
}

#endif /* KTHREAD_H_ */
