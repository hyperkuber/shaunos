/*
 * kthread.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <kernel/kthread.h>
#include <kernel/mm.h>
#include <asm/io.h>
#include <string.h>
#include <kernel/page.h>
#include <unistd.h>
#include <kernel/tss.h>
#include <kernel/kernel.h>
#include <kernel/tss.h>
#include <kernel/assert.h>
#include <kernel/cpu.h>
#include <kernel/vfs.h>
#include <asm/system.h>
#include <kernel/ext2.h>
#include <kernel/malloc.h>
#include <kernel/elf.h>
#include <kernel/pid.h>
#include <kernel/avltree.h>
#include <kernel/timer.h>
#include <kernel/console.h>
#include <kernel/resource.h>

u32 need_reschedule;
static u32 last_tid;
kthread_u_t *sysidle_thread;
struct kid_object kpid_obj;
struct avlnode *sys_proc = NULL;
extern u32 global_pg_dir[];
extern u32 kern_pgd_max;
extern u64 ticks;
extern u32 next_free_timer_id;
extern vnode_t *ext2_rootv;

void __attribute__((regparm(0))) thread_stub(void);

void wake_up(struct kernel_thread *kthread);
void wakeup_waiters(wait_queue_head_t *head);


u32 kget_current_pgd()
{
	return get_cr3();
}
u32 kset_current_pgd(u32 new_pgd)
{
	return load_cr3(new_pgd);
}

void init_thread(kthread_u_t *threadu, kprocess_t *host, int priority, int priv)
{
	threadu->kthread.priority = priority;
	threadu->kthread.priv = priv;
	threadu->kthread.ticks = 0;
	threadu->kthread.kstack = ((ulong_t *) threadu + sizeof(kthread_u_t));
	if (host == NULL)
		threadu->kthread.tid = last_tid++;
	else {
		threadu->kthread.tid = get_unused_id(&(host->p_tid_obj));
	}
	threadu->kthread.esp = ((u32)threadu + sizeof(kthread_u_t)) - 4;
	threadu->kthread.esp0 = threadu->kthread.esp - 8;//8 is for ss and esp
	if (priv == 3){
		threadu->kthread.host = host;
	} else
		threadu->kthread.host = NULL;	//for kernel thread
	threadu->kthread.flag |= KTHREAD_PROMPT;
	LOG_INFO("threadu:%x thread esp:%x",(ulong_t)threadu, threadu->kthread.esp);
	list_init(&(threadu->kthread.thread_list));
	list_init(&threadu->kthread.wq.waiter);
}

void inline push(struct kernel_thread *thread, ulong_t value)
{
	thread->esp -= 4;
	*(ulong_t *)thread->esp = value;
}

void shutdown_thread(void)
{
	LOG_INFO("shuting down thread, id:%d", current->tid);
	list_del(&current->thread_list);
	wakeup_waiters(&current->wq);
	if (current->priv == RPL_USER){
		kprocess_t *proc = current->host;
		if (proc){
			proc->p_threadtree = avl_delete(current->tid, proc->p_threadtree);
			put_id(&proc->p_tid_obj, current->tid);
		}
	}
	//free_pages(2, (u32)current);
	schedule();
}

void destroy_thread(kthread_u_t *threadu)
{
	list_del(&threadu->kthread.thread_list);
	//wakeup_waiters(&threadu->kthread.wq);
	//free_pages(1, (u32)current);
}


void shutdown_thread_tree(avltree_t *root)
{
	avltree_t *t;
	t = root;
	if (t){
		shutdown_thread_tree(t->right);
		shutdown_thread_tree(t->left);
		destroy_thread((kthread_u_t *)t->payload);
	}
}

void wakeup_waiters(wait_queue_head_t *head)
{
	kthread_t *t;
	struct linked_list *pos, *n;
	if (head){
		if (!list_is_empty(&head->waiter)){
			for (pos=head->waiter.next, n=pos->next;
					pos != &head->waiter; pos=n, n=n->next){
				t = container_of(pos, kthread_t, thread_list);
				//LOG_INFO("waking up thread:%x", (unsigned long)t);
				pt_wakeup(t);
			}

		}
	}
}


int kproc_free_pages(kprocess_t *proc)
{
	if (!proc)
		return -EINVAL;

	vm_t *vm = proc->mm.vm_page;
	vm += proc->mm.vm_max;

	for (; vm != proc->mm.vm_page; vm--) {
		if (vm->base == 0)
			continue;
		//LOG_INFO("freeing page:%x", vm->base);
		free_pages(get_order(vm->len), vm->base);
	}
	//LOG_INFO("freeing page:%x.", vm->base);
	free_pages(get_order(vm->len),(u32)vm->base);
	return 0;
}

int kclose_proc(kprocess_t *proc, int status)
{
	if (proc == NULL)
		return -EINVAL;
	int i;
	vm_t *vm;
	kprocess_t *parent = proc->p_parent;
	LOG_INFO("proc open files:%d", proc->open_files);
	for (i=0; i<proc->open_files; i++)
		vfs_close((file_t *)proc->file[i]);
	//while (1);
	if (proc->p_curr_cwd)
		vfs_close(proc->p_curr_cwd);

	vm = proc->mm.vm_page;
	if (proc == current->host)
		load_cr3(PA((u32)global_pg_dir));

	//shutdown all threads
	shutdown_thread_tree(proc->p_threadtree);
	//destroy thread tree
	avl_destroy_tree(proc->p_threadtree);

	sys_proc = avl_delete_node(proc->pid, sys_proc);

	kernel_id_map_destroy(&proc->p_tid_obj);
	kernel_id_map_destroy(&proc->p_fd_obj);

	//check for waiters, if exists, wake it up
	wakeup_waiters(&proc->p_wq);
	wakeup_waiters(proc->p_w4q);
	list_del(&proc->p_proc_list);

	if (parent->p_child == proc) {
		//if we are the only child
		if (list_is_empty(&proc->p_silbing))
			parent->p_child = NULL;
		else {
			parent->p_child = container_of(proc->p_silbing.next, kprocess_t, p_silbing);
			list_del(&proc->p_silbing);
		}
	}

	put_id(&kpid_obj, proc->pid);

	if (!(proc->p_flags & PROC_NOFRMS)) {
		kernel_id_map_destroy(&proc->p_fmid_obj);
	}

	if (proc->ems) {
		struct elf_module *em = proc->ems;
		struct elf_module *n = em->next;
		while (em) {
			if (em->dyaddr)
				kfree(em->dyaddr);
			kfree(em);
			if ((em=n) != NULL)
					n = n->next;
		}
	}

	kproc_free_pages(proc);

//	for (i=proc->mm.vm_idx-1;i>=0; i--){
//		free_pages(vm[i].len >> PAGE_SHIFT, vm[i].base);
//	}

	return 0;
}

void kexit(int status)
{
	int x;

	local_irq_save(&x);
	//list_del(&(current->thread_list));
	kprocess_t *proc = current->host;
	if (proc != NULL){
		kclose_proc(proc, status);
	}
	schedule();

}


void pt_timer_callback(void *data)
{
	pt_wakeup((struct kernel_thread *)data);
}

int kopen_process(int pid, kprocess_t **p)
{
	avltree_t *t;
	t = avl_find(pid, sys_proc);
	if (t != NULL){
		*p = (kprocess_t *)t->payload;
	} else
		return -ESRCH;
	return 0;
}

int kterminate_proc(int pid, int status)
{
	kprocess_t *proc;
	int retval;

	retval = kopen_process(pid, &proc);
	if (retval < 0)
		return retval;

	if (proc == current->host)
		return -EPERM;

	return kclose_proc(proc, status);
}


int ksleep(int ms)
{
	int ret = 0;
	int timer_id;
	int x;
	if (ms == 0){
		local_irq_save(&x);
		list_move_tail(&(run_queue[current->priority]), &current->thread_list);
		schedule();
		local_irq_restore(x);
		return 0;
	}
	if (ms < 0)
		return -EINVAL;

	ret = register_timer(ms, TIMER_FLAG_ONESHUT | TIMER_FLAG_ENABLED, pt_timer_callback, (u32)current);
	if (ret < 0)
		return ret;
	timer_id = ret;

	local_irq_save(&x);
	current->state = SLEEPING;
	list_move_tail(&(wait_queue[current->priority]), &current->thread_list);

	schedule();
	local_irq_restore(x);
	unregister_timer(timer_id);
	return 0;
}

int kwait(int *status)
{
	kprocess_t *proc, *child_proc;
	int x;
	wait_queue_head_t wq;
	struct linked_list *pos;
	list_init(&wq.waiter);
	proc = current->host;
	if (proc) {

		child_proc = proc->p_child;
		if (child_proc == NULL)
			return -ECHILD;

		local_irq_save(&x);
		list_del(&current->thread_list);
		list_add(&wq.waiter, &current->thread_list);
		child_proc->p_w4q = &wq;
		// we should wait for every child
		if (!list_is_empty(&child_proc->p_silbing)) {
			for (pos = child_proc->p_silbing.next; pos != &child_proc->p_silbing; pos = pos->next) {
				proc = container_of(pos, kprocess_t, p_silbing);
				if (proc) {
					proc->p_w4q = &wq;
				}
			}
		}
		schedule();
		local_irq_restore(x);
		return 0;
	}
	return -EOPNOTSUPP;
}
int kwait_pid(int pid, int ms)
{
	kprocess_t *proc;
	int retval, x;
	int timer_id;
	u32 expired;

	retval = kopen_process(pid, &proc);
	if (retval < 0){
		return retval;
	}

	if (ms == WAITE_INFINITE){
		local_irq_save(&x);
		list_del(&current->thread_list);
		list_add(&proc->p_wq.waiter, &current->thread_list);
		current->state = SLEEPING;
		schedule();
		local_irq_restore(x);
		return WAITE_OK;
	} else if (ms == 0){
		return WAITE_OK;
	} else {
		//ksleep(ms);
		local_irq_save(&x);
		list_del(&current->thread_list);
		list_add_tail(&proc->p_wq.waiter, &current->thread_list);
		current->state = SLEEPING;

		retval = register_timer(ms, TIMER_FLAG_ONESHUT | TIMER_FLAG_ENABLED, pt_timer_callback, (u32)current);
		if (retval < 0)
			return -ENOBUFS;

		timer_id = retval;

		schedule();
		local_irq_restore(x);

		retval = timer_get_expired(timer_id, &expired);
		if (retval < 0)
			return -retval;


		if (expired <= ticks){
			//timeout
			//LOG_INFO("t.expires:%d ticks:%d", t.expires, ticks);
			return WAITE_TIMEOUT;
		} else {
			//ret = t.expires - ticks;
			//LOG_INFO("t.expires:%d ticks:%d", t.expires, ticks);
			return WAITE_OK;
		}

	}

	return 0;

}

void setup_thread_stack(kthread_u_t *threadu, u32 entry, u32 arg, int priv)
{
	u32	cs,ds;
	u32 user_stack_top;
	u32 user_stack_base;
	u32 pa;
	kprocess_t *proc;
	u32 cr3;
	if (priv == RPL_USER){

		cs = USER_CS | RPL_USER;
		ds = USER_DS | RPL_USER;

		proc = threadu->kthread.host;

		user_stack_top = (u32)USER_STACK_LAZY ;//default stack size is 1M;
		user_stack_base = USER_STACK_BASE ;
		for (;user_stack_top < user_stack_base;){
			pa = PA(get_zero_page());
			vm_insert(proc, VA(pa), PAGE_SIZE, VM_STACK);
			map_page(user_stack_top, pa, proc->cr3, PAGE_PRE|PAGE_RW |PAGE_USR, proc);
			user_stack_top += 0x1000;
		}
		threadu->kthread.esp = user_stack_base;// stack grows downwards
		threadu->kthread.ustack = (u32 *)user_stack_base;
		if (!proc->mm.image.stack_start)
			proc->mm.image.stack_start = user_stack_base;

	}else{
		cs = KERNEL_CS |  RPL_KERN;
		ds = KERNEL_DS |  RPL_KERN;
		push(&threadu->kthread, arg);
	}

	if (priv == RPL_USER)
		cr3 = load_cr3(proc->cr3);

	push(&threadu->kthread, (u32)&shutdown_thread);

	if (priv == 3){
		push(&threadu->kthread, ds);
		push(&threadu->kthread, threadu->kthread.esp + 4);
	}

	push(&threadu->kthread, X86_EFLAGS_IF | X86_EFLAGS_SF | X86_EFLAGS_PF | 0x2);	//eflags ,  interrupt enable,
	push(&threadu->kthread, cs);
	push(&threadu->kthread, (u32)entry);

	push(&threadu->kthread, 0);	//eax
	push(&threadu->kthread, 0);	//ebx
	push(&threadu->kthread, 0);	//ecx
	push(&threadu->kthread, 0);	//edx
	push(&threadu->kthread, 0);	//esi
	push(&threadu->kthread, 0);	//edi
	push(&threadu->kthread, 0);	//ebp


	push(&threadu->kthread, ds);	//ds
	push(&threadu->kthread, ds);	//es
	push(&threadu->kthread, ds);	//fs
	push(&threadu->kthread, ds);	//gs

	threadu->kthread.eip = (u32)thread_stub;
	if (priv == RPL_USER)
		load_cr3(cr3);
}

void init_ldt(struct segment_descriptor *desc, ulong_t base, ulong_t size)
{
	desc->avl = 0;
	desc->db = 0;
	desc->dpl = 0;
	desc->present = 1;
	desc->reserved = 0;
	desc->granularity = 0;
	desc->system = 0;
	desc->type = 0x02;
	desc->base_high = (base >> 24) & 0xFF;
	desc->base_low = base & 0xFFFFFF;
	desc->limit_high = (size >> 16) & 0x0F;
	desc->limit_low = size & 0xFFFF;
}

kthread_u_t *
kcreate_thread(u32 entry, u32 arg, kprocess_t *host, kthread_att_t *thd_att)
{
	kthread_u_t *threadu = NULL;
	u32 tmp = 0;
	kthread_att_t att;

	threadu = (kthread_u_t *)alloc_pages(ALLOC_NORMAL | ALLOC_ZEROED, 1);
	if (threadu == NULL)
		return NULL;

	if (((u32)threadu & 0x1FFF) != 0){
		free_pages(1, (u32)threadu);
		tmp = (u32)alloc_pages(ALLOC_NORMAL, 2);
		if ((tmp & 0x1FFF) != 0) {
			threadu = (kthread_u_t *)(tmp+PAGE_SIZE);
		} else {
			threadu = (kthread_u_t *)tmp;
		}
		vm_insert(host, tmp, 4 << PAGE_SHIFT, VM_THREADU);
		goto next;
	}
	vm_insert(host, (u32)threadu, 2<<PAGE_SHIFT, VM_THREADU);
next:
	if (thd_att == NULL){
		att.priority = 0;
		att.priv = 0;
		att.state = RUNNING;
	} else {
		att.priority = thd_att->priority;
		att.priv = thd_att->priv;
		att.state = thd_att->state;
	}
	init_thread(threadu, host, att.priority, att.priv);

	setup_thread_stack(threadu, entry, arg, att.priv);

	switch (att.state){
	case SUSPEND:
	case STOPED:
	case SLEEPING:
		list_add_tail(&(wait_queue[att.priority]), &(threadu->kthread.thread_list));
		break;
	case RUNNING:
		list_add_tail(&(run_queue[att.priority]), &(threadu->kthread.thread_list));
		LOG_INFO("add thread:%x to run queue", (u32)threadu);
		break;
	default:
		LOG_INFO("unknow thread state, add thread to run queue by default");
		list_add_tail(&(run_queue[att.priority]), &(threadu->kthread.thread_list));
		break;
	}

	return threadu;
}

void* sysidle(void * args)
{
	while (1){
		need_reschedule = 1;
		cpu_relax();
	};

	return NULL;
}


void pt_wakeup(struct kernel_thread *kthread)
{
	int x;
	local_irq_save(&x);
	kthread->state = RUNNING;
	list_move_head(&(run_queue[kthread->priority]), &kthread->thread_list);
	local_irq_restore(x);
}

kthread_t* __attribute__((regparm(3))) __switch_to(kthread_t *prev, kthread_t *next)
{
	set_kernel_stack(next->esp0);
	return prev;
}

int context_switch(kthread_t *prev, kthread_t *next)
{
	kprocess_t *p_curr, *p_next;
	u32 esi,edi;
	p_curr = prev->host;
	p_next = next->host;
	if (prev == next)
		return 0;
	if (p_curr != p_next){
		if (p_next != NULL){
			//switch mm, reload cr3
			//LOG_INFO("switching to different process, cr3:%x threadu:%x", p_next->cr3, (unsigned long)next);
			load_cr3(p_next->cr3);

		}
		// if p_next == NULL,  we maybe switch to a kernel thread from a process
	}

	//switch_to(curr, next);
	asm volatile("pushfl\n\t"
		     "pushl %%ebp\n\t"
		     "movl %%esp,%0\n\t"	/* save ESP */
		     "movl %5,%%esp\n\t"	/* restore ESP */
		     "movl $1f,%1\n\t"		/* save EIP */
		     "pushl %6\n\t"		/* restore EIP */
		     "jmp __switch_to\n"
		     "1:\t"
		     "popl %%ebp\n\t"
		     "popfl"
		     :"=m" (prev->esp),"=m" (prev->eip),
		      "=a" (prev),"=S" (esi),"=D" (edi)
		     :"m" (next->esp),"m" (next->eip),
		      "2" (prev), "d" (next));

	return 0;

}



/*
 * a simple schedule algorithm.
 * Priority-Based Round Robin
 */

void schedule(void)
{
	kthread_t *prev, *next;
	struct linked_list *list_ptr;
	prev = current;
	int i = 5;
	int x;

	local_irq_save(&x);

	need_reschedule = 0;


	/*
	 * first, choose the highest priority
	 */
	while (--i >= 0){
		if (list_is_empty(&(run_queue[i])))
		{
			if (i != 0)
				continue;
			else/* here we should choose the system idle,in theory*/
				break;
		}
		break;
	}

	/*
	 * we choose the first thread of the run_queue,
	 * and make it current, then move it to the last
	 * of the queue
	 */
	list_ptr = run_queue[i].next;
	next = container_of(list_ptr, kthread_t, thread_list);

	list_del(list_ptr);
	list_add_tail(&run_queue[i], list_ptr);

	if (prev == next) {
		local_irq_restore(x);
		return;
	}

	context_switch(prev, next);

	local_irq_restore(x);

}

int fd_attach(int fd, kprocess_t *proc, file_t *file)
{
	LOG_INFO("in. fd:%d", fd);
	if (fd < 0)
		return -EINVAL;
	if (fd >= 1024)
		return -ENFILE;

	if (((u32 *)proc->file)[fd])
		return -EBADF;

	((u32*)proc->file)[fd] = (u32)file;
	LOG_INFO("end");
	return fd;
}

int fd_detach(int fd, kprocess_t *proc)
{
	LOG_INFO("in, fd:%d", fd);
	if (fd < 0)
		return -EINVAL;

	if (!((u32*)proc->file)[fd])
		return -EBADF;

	((u32 *)proc->file)[fd] = 0;
	return 0;
}

int load_binary(kprocess_t *proc, char *path, u32 *entry)
{
	int retval;
	file_t *file;
	char *data;
	u32 saved_cr3;
	int fd;
	kprocess_t *parent = get_current_process();
	retval = vfs_open(proc->p_curr_cwd->f_vnode, path, O_RDWR, &file);
	if (retval < 0){
		LOG_ERROR("open file:%s failed", path);
		return -ENOENT;
	}

	if ((file->f_vnode->v_mode & U_EXE) == 0){
		LOG_ERROR("file %s has no execute permission(Only check USER)", path);
		vfs_close(file);
		return -EACCES;
	}
	fd = get_unused_id(&(proc->p_fd_obj));
	fd_attach(fd, proc, file);
	proc->open_files++;

	data = (char *)kmalloc(128);
	assert(data != NULL);
	memset(data, 0, 128);

	retval = vfs_read(file, (void *)data, 128);
	if (retval < 0){
		LOG_ERROR("read file:%s error", path);
		goto end;
	}
//	pgd = kset_current_pgd(proc->cr3);
	if (parent != NULL) {
		saved_cr3 = parent->cr3;
		parent->cr3 = proc->cr3;
		kset_current_pgd(proc->cr3);
	} else {
		saved_cr3 = kset_current_pgd(proc->cr3);
	}

	retval = load_elf(proc, file, data, entry);
	if (retval < 0){
		LOG_ERROR("load elf failed");
		goto read_out;
	}
	LOG_NOTE("load elf ok, entry:%x", *entry);

read_out:
	if (parent) {
		parent->cr3 = saved_cr3;
	}
	kset_current_pgd(saved_cr3);
end:
	kfree(data);
	vfs_close(file);
	fd_detach(fd, proc);
	put_id(&proc->p_fd_obj, fd);
	proc->open_files--;
	return retval;
}

void vm_insert(kprocess_t *proc, u32 base, u32 len, u32 flag)
{
	if (proc == NULL)
		return;

	vm_t *vm;
	u32 idx;
	idx = 0;
	vm = proc->mm.vm_page;

	for (; idx < proc->mm.vm_max; idx++) {
		if (vm[idx].base == 0) {
			vm[idx].base = base;
			vm[idx].len = len;
			vm[idx].flag = flag;
			//proc->mm.vm_idx++;
			return;
		}
	}
	if (idx == proc->mm.vm_max){
		LOG_ERROR("idx:%d.", idx);
		BUG();
	}

}

vm_t *vm_search_flag(kprocess_t *proc, u32 flag)
{
	if (!proc)
		return NULL;

	vm_t *vm = proc->mm.vm_page;
	int idx;
	for (idx=0; idx < proc->mm.vm_max; idx++){
		if (vm->flag & flag)
			break;
		vm++;
	}

	return vm;
}

int count(char **p)
{
	int i = 0;
	while (p[i]){
		if (i > 64)
			return -E2BIG;
		i++;
	}
	return i;
}

int probe_for_read(u32 addr, int size)
{
	if (size < 0)
		return -EINVAL;
	if (size == 0)
		return 0;

	if (addr + size > __PAGE_OFFSET)
		return -EFAULT;
	return 0;
}


/*
 * do not use this for kernel mode address
 */
int probe_for_write(u32 addr, int size)
{
	u32 *pgd;

	//pgd =(unsigned long *)VA(get_current_process()->cr3);
	pgd =(u32 *)VA(get_cr3());
	if (addr + size > __PAGE_OFFSET)
		return -1;
	//get page directory entry, ie, page table address
	u32 pde = *(pgd + PDE_INDEX(addr));
	if (!pde)
		return -1;
	if (!(pde & PAGE_PRE))
		return -1;

//	if (!(pde & PAGE_RW))
//		return -1;

	if (!(pde & PAGE_PSE)){
		//get page table entry, that is what we want , physical address
		u32 pte = *((u32 *)(VA(pde) & PAGE_MASK) + PTE_INDEX(addr));
		if (!pte)
			return -1;
//		if (!(pte & PAGE_RW))
//			return -1;
		if (!(pte & PAGE_PRE))
			return -1;
	}

	return 0;

}

int copy_from_user(char *dest, char *src, int size)
{
	if (size < 0)
		return -EINVAL;
	if (size == 0)
		return 0;
	if (!dest || !src)
		return 0;

	if (probe_for_read((u32)src, size) < 0)
		return -EFAULT;

	u8 *d = (u8 *)dest;
	u8 *s = (u8 *)src;

	switch (size){
	case 1:
		*d = *s;
		break;
	case 2:
		*(u16 *)d = *(u16 *)s;
		break;
	case 4:
		*(u32 *)d = *(u32 *)s;
		break;
	case 6:
		*(u32 *)d = *(u32 *)s;
		d += 4;
		s += 4;
		*(u16 *)d = *(u16 *)s;
		break;
	case 8:
		*(u32 *)d = *(u32 *)s;
		d += 4;
		s += 4;
		*(u32 *)d = *(u32 *)s;
		break;
	default:
		memcpy(d, s, size);
		break;
	}

	return 0;
}

int copy_to_user(char *dest, char *src, int size)
{
	int i;
	int nr_pg;
	u32 addr;
	int ret;
	if (size < 0)
		return -EINVAL;
	if (size == 0)
		return 0;
	if (!dest || !src)
		return 0;
	//LOG_INFO("in");
	nr_pg = PAGE_ALIGN(size) >> PAGE_SHIFT;
	for (i=0; i<nr_pg; i++) {
		addr = (u32)dest + (i<<PAGE_SHIFT);
		ret  = probe_for_write(addr, PAGE_SIZE);
		if (ret < 0) {
			LOG_ERROR("addr:%x does not exist, cr3:%x", addr, (u32)get_cr3());
			BUG();
			return -EFAULT;
		}
	}


	u8 *d = (u8 *)dest;
	u8 *s = (u8 *)src;

	switch (size){
	case 1:
		*d = *s;
		break;
	case 2:
		*(u16 *)d = *(u16 *)s;
		break;
	case 4:
		*(u32 *)d = *(u32 *)s;
		break;
	case 6:
		*(u32 *)d = *(u32 *)s;
		d += 4;
		s += 4;
		*(u16 *)d = *(u16 *)s;
		break;
	case 8:
		*(u32 *)d = *(u32 *)s;
		d += 4;
		s += 4;
		*(u32 *)d = *(u32 *)s;
		break;
	default:
		memcpy(d, s, size);
		break;
	}
	return size;
}

int copy_to_proc(kprocess_t *proc, char *dest, char *src, int size)
{
//	u32 cr3;
//	int x;
	int ret;
//	local_irq_save(&x);
//	cr3 = get_cr3();
//	if (cr3 != proc->cr3)
//		cr3 = load_cr3(proc->cr3);

	ret = copy_to_user(dest, src, size);
	if (ret < 0)
		ret = -EFAULT;
//	LOG_INFO("restore old cr3:%x, pro cr3:%x", cr3, proc->cr3);
//	if (cr3 != proc->cr3)
//		load_cr3(cr3);
//	local_irq_restore(x);
	return ret;
}

int copy_from_proc(kprocess_t *proc, char *dest, char *src, int size)
{
//	u32 cr3;
//	int x;
	int ret;
//	local_irq_save(&x);
//	cr3 = get_cr3();
//	if (cr3 != proc->cr3)
//		cr3 = load_cr3(proc->cr3);

	ret = copy_from_user(dest, src, size);
	if (ret < 0)
		ret = -EFAULT;
	//LOG_INFO("restore old cr3:%x, pro cr3:%x", cr3, proc->cr3);
//	if (cr3 != proc->cr3)
//		load_cr3(cr3);
//	local_irq_restore(x);
	return ret;
}

int copy_strings(int argc, char **argv, char *dest, char **p0, char *baddr)
{
	int i, len, offset = 0;
	u32 end = PAGE_ALIGN((u32)dest);

	for (i=0; i<argc; i++){
		char *str = argv[i];
		if (str == NULL)
			break;
		len = strlen(str) + 1;
		LOG_INFO("copy str:%s", str);
		if ((u32)(dest + offset) > end)
			return -E2BIG;
		copy_from_user(dest+offset, str, len);
		*p0++ = (baddr + ((u32)(dest+offset) & 0xFFF));
		offset += len;
	}

	return 0;
}

int setup_arg_env(kprocess_t *proc, char **argv, char **envp)
{
	int argc, envc;
	u32 addr;
	char *p;
	char **p0;
	if (argv){
		LOG_INFO("setup proc argv");
		argc = count(argv);
		if (argc < 0)
			return argc;	//should be e2big
		argc++;
		addr = get_zero_page();
		assert(addr != 0);
		p = (char *)addr;
		*((int *)p) = argc;
		p += sizeof(int);
		p0 = (char **)p;
		p += 64 * sizeof(int);
		strncpy(p, proc->p_name, sizeof(proc->p_name));
		*p0++ = (char *)(ARG_BASE + ((u32)p & 0xFFF));
		p += strlen(proc->p_name);
		LOG_INFO("copy args, argc:%d", argc);
		copy_strings(argc, argv, ++p, p0, (char *)ARG_BASE);
		LOG_INFO("map arg base page:%x", ARG_BASE);
		map_page(ARG_BASE, PA(addr), proc->cr3, PAGE_PRE|PAGE_RW | PAGE_USR, proc);
		vm_insert(proc, addr, PAGE_SIZE, VM_COMMON);
		proc->mm.image.arg_start = ARG_BASE;
	}

	if (envp){
		LOG_INFO("setup proc envp");
		envc = count(envp);
		if (envc < 0)
			return envc;
		addr = get_zero_page();
		assert(addr != 0);
		p = (char *)addr;
		p0 = (char **)p;
		p += 64 * sizeof(int);
		copy_strings(envc, envp, p, p0, (char *)ENV_BASE);
		LOG_INFO("map env base page:%x", ENV_BASE);
		map_page(ENV_BASE, PA(addr), proc->cr3, PAGE_PRE|PAGE_RW | PAGE_USR, proc);
		vm_insert(proc, addr, PAGE_SIZE, VM_COMMON);
		proc->mm.image.env_start = ENV_BASE;
	}
	LOG_INFO("end");
	return 0;
}

void kproc_init_limit(kprocess_t *proc)
{
#define INIT_RLIMITS							\
{									\
	[RLIMIT_CPU]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_FSIZE]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_DATA]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_STACK]		= {      _STK_LIM, _STK_LIM_MAX  },	\
	[RLIMIT_CORE]		= {             0, RLIM_INFINITY },	\
	[RLIMIT_RSS]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_NPROC]		= {             0,             0 },	\
	[RLIMIT_NOFILE]		= {      INR_OPEN,     INR_OPEN  },	\
	[RLIMIT_MEMLOCK]	= {   MLOCK_LIMIT,   MLOCK_LIMIT },	\
	[RLIMIT_AS]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_LOCKS]		= { RLIM_INFINITY, RLIM_INFINITY },	\
	[RLIMIT_SIGPENDING]	= { MAX_SIGPENDING, MAX_SIGPENDING },	\
	[RLIMIT_MSGQUEUE]	= { MQ_BYTES_MAX, MQ_BYTES_MAX },	\
}
	struct rlimit *r = proc->lmt;

	r[RLIMIT_CPU].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_CPU].rlim_max = RLIM_INFINITY;
	r[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_FSIZE].rlim_max = RLIM_INFINITY;
	r[RLIMIT_DATA].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_DATA].rlim_max = RLIM_INFINITY;
	r[RLIMIT_STACK].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_STACK].rlim_max = RLIM_INFINITY;
	r[RLIMIT_CORE].rlim_cur = 0;
	r[RLIMIT_CORE].rlim_max = RLIM_INFINITY;
	r[RLIMIT_RSS].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_RSS].rlim_max = RLIM_INFINITY;
	r[RLIMIT_NPROC].rlim_cur = 0;
	r[RLIMIT_NPROC].rlim_max = 0;
	r[RLIMIT_NOFILE].rlim_cur = 1024;
	r[RLIMIT_NOFILE].rlim_max = 1024;
	r[RLIMIT_MEMLOCK].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_MEMLOCK].rlim_max = RLIM_INFINITY;
	r[RLIMIT_AS].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_AS].rlim_max = RLIM_INFINITY;
	r[RLIMIT_LOCKS].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_LOCKS].rlim_max = RLIM_INFINITY;
	r[RLIMIT_SIGPENDING].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_SIGPENDING].rlim_max = RLIM_INFINITY;
	r[RLIMIT_MSGQUEUE].rlim_cur = RLIM_INFINITY;
	r[RLIMIT_MSGQUEUE].rlim_max = RLIM_INFINITY;


}

int kcreate_process(char *path, char *argv[], char **env, kthread_att_t *att, u32 flag)
{
	int i, retval = -1;
	u32 *pgd;
	u32 entry = 0;
	kprocess_t *proc, *parent;
	kproc_t *chld_proc;
	proc = (kprocess_t *)get_zero_page();
	assert(proc != NULL);

	proc->mm.vm_page = (vm_t *)get_zero_page();
	proc->mm.vm_max = PAGE_SIZE / sizeof(vm_t);
	vm_insert(proc, (u32)proc, PAGE_SIZE, VM_PROC);
	vm_insert(proc, (u32)proc->mm.vm_page, PAGE_SIZE, VM_RSVD);

	proc->pid = get_unused_id(&kpid_obj);
	list_init(&proc->p_wq.waiter);
	list_init(&proc->p_silbing);
	list_init(&proc->p_proc_list);
	//list_init(&proc->p_child);



	pgd =(u32 *)get_zero_page();
	vm_insert(proc, (u32)pgd, PAGE_SIZE, VM_PGD);

	for (i=(__PAGE_OFFSET >> 22); i<1024; i++){
		*(pgd+i) = global_pg_dir[i];
	}
	*pgd = global_pg_dir[0];//copy first 4M mapping, kernel mode page, user can not access it!

	proc->cr3 = PA((u32)pgd);

	parent = get_current_process();
	if (parent == NULL){
		proc->p_currdir = vfs_get_rootv();
		proc->ppid = 1;
		proc->gid = 1;
		retval = vfs_open(vfs_get_rootv(), "/", O_RDWR, &proc->p_curr_cwd);
		if (retval < 0)
			goto load_out;
	} else {
		proc->ppid = parent->pid;
		proc->gid = parent->gid;
		proc->p_currdir = parent->p_currdir;
		proc->p_parent = parent;
		proc->p_curr_cwd = parent->p_curr_cwd;
		parent->p_curr_cwd->f_refcnt++;
	}

	proc->file =(u32 **)get_zero_page();
	vm_insert(proc, (u32)proc->file, PAGE_SIZE, VM_FILE);

	// default fds and total threads  1024
	kernel_id_map_init(&(proc->p_tid_obj), 128);
	kernel_id_map_init(&(proc->p_fd_obj), 128);

	if (!(flag & PROC_NOFRMS)) {
		kernel_id_map_init(&(proc->p_fmid_obj), 128);
		proc->frame =(u32 **)get_zero_page();
		vm_insert(proc, (u32)proc->frame, PAGE_SIZE, VM_FRAME);
	}

	if (parent != NULL) {
		memcpy((void *)proc->file, (void *)parent->file, parent->open_files * sizeof(u32));
		for (i=0; i<parent->open_files; i++)
			((file_t *)proc->file[i])->f_refcnt++;
		//we should take care of FD_CLOEXEC
		//here, the child just inherit fds opened by parent, including 0,1,2,which
		// conflicts with POSIX.1-2001
		kid_dup(&proc->p_fd_obj, &parent->p_fd_obj);
		proc->open_files = parent->open_files;
	}

	strncpy((char *)proc->p_name, path, PROC_NAME_MAX);

	kproc_init_limit(proc);

	retval = setup_arg_env(proc, argv, env);
	if (retval < 0){
		LOG_ERROR("setup environment failed");
		goto load_out;
	}

	retval = load_binary(proc, path, &entry);
	if (retval < 0){
		LOG_ERROR("load binary error");
		goto load_out;
	}


	if (parent != NULL){
		list_add(&parent->p_proc_list, &proc->p_proc_list);
		//list_add(&parent->p_child, proc->)
		if (parent->p_child != NULL) {
			chld_proc = parent->p_child;
			list_add(&chld_proc->p_silbing, &proc->p_silbing);
		} else {
			parent->p_child = proc;
		}
	}

	LOG_INFO("proc open files:%d", proc->open_files);
	//load_cr3(old_cr3);

	proc->p_flags |= flag;
	//create main thread
	kthread_u_t *thread = kcreate_thread(entry, (u32)argv, proc, att);
	assert(thread != NULL);
	proc->p_threadtree = avl_insert(thread->kthread.tid, (void *)thread, proc->p_threadtree);
	sys_proc = avl_insert(proc->pid, (void *)proc, sys_proc);

	retval = proc->pid;
	LOG_INFO("create process end");
	return retval;

load_out:
	put_id(&kpid_obj, proc->pid);
	kernel_id_map_destroy(&(proc->p_tid_obj));
	kernel_id_map_destroy(&(proc->p_fd_obj));
	if (!(flag & PROC_NOFRMS)) {
		kernel_id_map_destroy(&proc->p_fmid_obj);
	}
	kproc_free_pages(proc);

//	for (i=proc->mm.vm_idx-1; i>=0; i--){
//		entry = proc->mm.vm_page[i].base;
//		if (entry){
//			free_pages(1, entry);
//		}
//	}
	return retval;
}



int kexecve(char *filename, char **argv, char **env)
{
	kthread_att_t att;
	int ret = 0;
	att.priority = 0;
	att.priv = 3;
	att.state = RUNNING;
	int path_len = strnlen_user(filename, PATH_MAX) + 1;
	if (path_len < 0)
		return -EFAULT;
	char *path = (char *)kmalloc(path_len);
	if (path == NULL)
		return -ENOMEM;

	copy_from_user(path, filename, path_len);

	int pid = kcreate_process(path, argv, env, &att, 0);
	if (pid < 0){
		ret = pid;
		goto END;
	}

END:
	kfree(path);
	LOG_INFO("execve out.pid:%d", pid);
	return ret;
}

int kgetpid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->pid;

	return -ESRCH;
}
int kgetppid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->ppid;
	return -ESRCH;
}

int kgetuid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->uid;
	return -ESRCH;
}

int kgeteuid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->euid;
	return -ESRCH;
}

int kgetgid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->gid;
	return -ESRCH;
}
int kgetegid(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->egid;
	return -ESRCH;
}

int kgetpgid(int pid)
{
	kprocess_t *proc = current->host;
	kprocess_t *p;
	int ret;
	if (proc) {
		if (pid == 0)
			return proc->gid;
		else {
			ret = kopen_process(pid, &p);
			if (ret < 0)
				return -ESRCH;
			else {
				return p->gid;
			}
		}
	}

	return -ESRCH;
}

int kgetpgrp(void)
{
	kprocess_t *proc = current->host;
	if (proc)
		return proc->gid;
	return -ESRCH;
}

int ksetpgid(int pid, int pgid)
{
	kprocess_t *proc = current->host;
	kprocess_t *p;
	int ret;
	if (proc) {
		if (pid == 0) {
			proc->gid = pgid;
			return 0;
		} else {
			ret = kopen_process(pid, &p);
			if (ret < 0)
				return -ESRCH;
			p->gid = pgid;
			return 0;
		}
	}
	return -ESRCH;
}


int kseteuid(int euid)
{
	kprocess_t *proc = current->host;

	if (!proc)
		return -ENOSYS;

	proc->euid = euid;
	return 0;
}

int ksetegid(int egid)
{
	kprocess_t *proc = current->host;

	if (!proc)
		return -ENOSYS;
	proc->egid = egid;
	return 0;
}


int ktcgetpgrp(int fd)
{
	kprocess_t *proc = current->host;
	if (proc) {
		file_t *fp = (file_t *)proc->file[fd];
		if (!fp)
			return -EBADF;

		if (fp->f_type & FTYPE_TTY) {
			return proc->gid;
		}

		return -ENOTTY;
	}

	return -ENOSYS;
}


int ksbrk(int incr)
{
	kprocess_t *proc;
	proc = get_current_process();
	u32 curr_brk = proc->mm.image.brk;
	if (incr == 0)
		return curr_brk;
	u32 end_brk = PAGE_ALIGN(curr_brk + incr);
	u32 addr;
	int page_num = (end_brk - curr_brk) >> PAGE_SHIFT;
	while (page_num-- > 0){
		addr = get_zero_page();
		map_page(curr_brk, PA(addr), proc->cr3, PAGE_PRE|PAGE_RW|PAGE_USR, proc);
		vm_insert(proc, addr, PAGE_SIZE, VM_COMMON);
		curr_brk += PAGE_SIZE;
	}
	curr_brk = proc->mm.image.brk;
	proc->mm.image.brk = end_brk;
	return curr_brk;
}

int kisatty(int fd)
{
	kprocess_t *proc = current->host;
	file_t *file = (file_t *)proc->file[fd];
	if (!file)
		return -EBADF;
	if (S_ISCHR(file->f_type))
		return 1;
	else
		return 0;
}



int kgetpriority(int which, id_t who)
{
	int ret;
	int pgrp;
	int uid;
	kprocess_t *proc = current->host;
	struct linked_list *pos;
	kprocess_t *p;
	if (which < PRIO_PROCESS || which > PRIO_USER)
		return -EINVAL;

	if (proc == NULL){
		//kernel thread.
		return current->priority;
	}
	ret = -ESRCH;;
	switch (which){
	case PRIO_PROCESS:
		if (!who)
			ret = proc->nice;
		break;
	case PRIO_PGRP:
		if (!who) {
			pgrp = proc->gid;
			ret = proc->nice;
		}
		else {
			pgrp = who;
			ret = 0;
		}
		for (pos = proc->p_proc_list.next; pos != &proc->p_proc_list; pos = pos->next){
				p = container_of(pos, kprocess_t, p_proc_list);
				if (p->gid == pgrp) {
					if (ret < (20 - p->nice))
						ret = (20 - p->nice);
				}
		}
		break;
	case PRIO_USER:
		if (!who){
			uid = proc->uid;
			ret = proc->nice;
		} else {
			uid = who;
			ret = 0;
		}
		for (pos = proc->p_proc_list.next; pos != &proc->p_proc_list; pos = pos->next){
				p = container_of(pos, kprocess_t, p_proc_list);
				if (p->uid == uid) {
					if (ret < (20 - p->nice))
						ret = (20 - p->nice);
				}
		}
		break;
	}

	return ret;
}

int ksetpriority(int which, id_t who, int value)
{
	int ret;
	int gid, uid;
	kprocess_t *proc = current->host;
	kprocess_t *p;
	struct linked_list *pos;

	if (which < 0 || which > 2)
		return -EINVAL;

	if (value > 20)
		value = 20;
	if (value < -20)
		value = -20;

	ret = -ESRCH;
	switch (which) {
	case PRIO_PROCESS:
		if (!who)
			proc->nice = value;
		else {
			for (pos = proc->p_proc_list.next; pos != &proc->p_proc_list; pos=pos->next){
				p = container_of(pos, kprocess_t, p_proc_list);
				if (p->pid == who) {
					p->nice = value;
					ret = 0;
				}
			}
		}
		break;
	case PRIO_PGRP:
		if (!who) {
			gid = proc->gid;
			proc->nice = value;
		}
		else
			gid = who;

		for (pos = proc->p_proc_list.next; pos != &proc->p_proc_list; pos=pos->next){
			p = container_of(pos, kprocess_t, p_proc_list);
			if (p->gid == gid) {
				p->nice = value;
				ret = 0;
			}
		}
		break;
	case PRIO_USER:
		if (!who) {
			uid = proc->uid;
			proc->nice = value;
		}
		else
			uid = who;
		for (pos = proc->p_proc_list.next; pos != &proc->p_proc_list; pos=pos->next){
			p = container_of(pos, kprocess_t, p_proc_list);
			if (p->uid == uid) {
				p->nice = value;
				ret = 0;
			}
		}
		break;
	}

	return ret;
}


int kgetrusage(int who, struct rusage *r_usage)
{
	int ret = 0;
	kprocess_t *proc = current->host;

	if (!proc)
		return -ENOSYS;

	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;

	if (!r_usage)
		return -EINVAL;

	switch (who) {
	case RUSAGE_SELF:
		memcpy((char *)r_usage, (char *)&proc->rs, sizeof(struct rusage));
		break;
	case RUSAGE_CHILDREN:
		if (!proc->p_child)
			break;
		if (!proc->p_child->p_w4q)
			break;
		/*
		 * we should sum all children 's usages up?
		 */

		memcpy((char *)r_usage, (char *)&proc->p_child->rs, sizeof(struct rusage));
		break;
	default:
		BUG();
	}
	return ret;
}

int kgetrlimit(int resource, struct rlimit *r_limt)
{

	kprocess_t *proc = current->host;
	if (!proc)
		return -ENOSYS;

	if (resource >= RLIM_NLIMITS || resource < 0)
		return -EINVAL;

	if (!r_limt)
		return -EINVAL;

	r_limt->rlim_cur = proc->lmt[resource].rlim_cur;
	r_limt->rlim_max = proc->lmt[resource].rlim_max;

	return 0;
}

int ksetrlimit(int resource, struct rlimit *r_limt)
{
	kprocess_t *proc = current->host;
	if (!proc)
		return -ENOSYS;

	if (resource >= RLIM_NLIMITS || resource < 0)
		return -EINVAL;

	if (!r_limt)
		return -EINVAL;

	proc->lmt[resource].rlim_cur = r_limt->rlim_cur;
	proc->lmt[resource].rlim_max = r_limt->rlim_max;

	return 0;
}

void schedule_init(void)
{

	int i;
	for (i=0; i<MAX_PRIORITY; i++){
		list_init(&(run_queue[i]));
		list_init(&(wait_queue[i]));
	}

	kernel_id_map_init(&kpid_obj, PAGE_SIZE);

	sysidle_thread = kcreate_thread((u32)&sysidle, 0, NULL, NULL);
	assert(sysidle != NULL);

}
