/*
 * syscall.c
 *
 * Copyright (c) 2012-2015 Shaun Yuan
 *
 */

#include <kernel/syscall.h>
#include <kernel/kernel.h>
#include <unistd.h>
#include <kernel/kthread.h>
#include <kernel/timer.h>
#include <kernel/idt.h>
#include <kernel/vfs.h>
#include <string.h>
#include <kernel/stat.h>
#include <kernel/types.h>
#include <kernel/time.h>
#include <kernel/malloc.h>
#include <kernel/net/uipc_syscall.h>
#include <kernel/uio.h>
#include <kernel/sys/iocommon.h>
#include <kernel/gfx/gfx.h>
#include <kernel/elf.h>
#include <kernel/resource.h>
#include <kernel/limits.h>

typedef	int (*syscall)(trap_frame_t *regs);

int sys_open(trap_frame_t *regs)
{
	char *up = (char *)regs->ebx;
	int ret;
	if (up == NULL)
		return -EINVAL;

	if (strlen_user(up) > PATH_MAX){
		return -ENAMETOOLONG;
	}
	char *path = vfs_get_name(up);
	if (!path)
		return -EFAULT;

	int flags = regs->ecx;
	if (flags < 0)
		return -EINVAL;

	int mode = 0;
	if (flags & O_CREAT){
		mode = regs->edx;
	}

	if (mode < 0)
		return -EINVAL;

	ret = kopen(path, flags, mode);

	vfs_put_name(path);
	return ret;
}

int sys_read(trap_frame_t *regs)
{
	int fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;

	unsigned char *buf = (unsigned char *)regs->ecx;
	if (buf == NULL)
		return -EINVAL;
	unsigned long len = regs->edx;
	if (len > SSIZE_MAX)
		return -EINVAL;
	if (len == 0)
		return 0;

	return kread(fd, buf, len);
}

int sys_readv(trap_frame_t *regs)
{
	int fd = regs->ebx;
	struct iovec *uiovec;
	int cnt;

	if (fd < 0)
		return -EINVAL;

	cnt = regs->edx;
	if (cnt < 0)
		return -EINVAL;

	uiovec = (struct iovec *)regs->ecx;

	if (!uiovec)
		return -EINVAL;

	return kreadv(fd, uiovec, cnt);
}

int sys_write(trap_frame_t *regs)
{
	int fd = regs->ebx;
//	if (fd == 0)
//		return printk("%s", regs->ecx);
	if (fd < 0)
		return -EINVAL;
	char *buf = (char *)regs->ecx;
	if (buf == NULL)
		return -EINVAL;
	unsigned long len = regs->edx;
	if (len > SSIZE_MAX)
		return -EINVAL;
	if (len == 0)
		return 0;

	return kwrite(fd, buf, len);
}

int sys_writev(trap_frame_t *regs)
{
	int fd = regs->ebx;
	struct iovec *uiovec;
	int cnt;

	if (fd < 0)
		return -EINVAL;

	cnt = regs->edx;
	if (cnt < 0)
		return -EINVAL;

	uiovec = (struct iovec *)regs->ecx;

	if (!uiovec)
		return -EINVAL;

	return kwritev(fd, uiovec, cnt);
}

int sys_exit(trap_frame_t *regs)
{
	kexit(regs->ecx);
	return 0;
}


int sys_close(trap_frame_t *regs)
{
	if (regs->ecx < 0)
		return -EINVAL;

	return kclose(regs->ecx);;
}

int sys_sleep(trap_frame_t *regs)
{
	if (regs->ebx < 0){
		return -EINVAL;
	}
	return ksleep(regs->ebx * 1000);
}

int sys_wait(trap_frame_t *regs)
{
	int *status = (int *)regs->ecx;
	return kwait(status);
}

int sys_wait_pid(trap_frame_t *regs)
{
	int pid = regs->ebx;
	int timeout = regs->ecx;

	if (pid < 0 || timeout < 0)
		return -EINVAL;


	return kwait_pid(pid, timeout);
}

int sys_execve(trap_frame_t *regs)
{
	char *name = (char *)regs->ebx;
	if (!name)
		return -EINVAL;
	char **argv = (char **)regs->ecx;
	char **env = (char **)regs->edx;

	return kexecve(name, argv, env);
}

int sys_sbrk(trap_frame_t *regs)
{
	int incr = (int)regs->ecx;
	if (incr < 0)
		return -EINVAL;

	return ksbrk(incr);
}

int sys_fstat(trap_frame_t *regs)
{
	int ret;
	int fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;
	struct stat st;
	struct stat *ust = (struct stat *)regs->ecx;
	if (!ust)
		return -EINVAL;

	memset((char *)&st, 0, sizeof(st));
	ret = kfstat(fd, &st);
	if (ret < 0)
		return ret;
	ret = copy_to_user((char *)ust, (char *)&st, sizeof(st));
	if (ret < 0)
		return ret;

	return 0;
}

int sys_fork(trap_frame_t *regs)
{
	return 800;
}

int sys_getpid(trap_frame_t *regs)
{
	return kgetpid();
}
int sys_getppid(trap_frame_t *regs)
{
	return kgetppid();
}

int sys_getuid(trap_frame_t *regs)
{
	return kgetuid();
}

int sys_geteuid(trap_frame_t *regs)
{
	return kgeteuid();
}
int sys_getgid(trap_frame_t *regs)
{
	return kgetgid();
}
int sys_getegid(trap_frame_t *regs)
{
	return kgetegid();
}

int sys_getpgid(trap_frame_t *regs)
{
	return kgetpgid(regs->ebx);
}
int sys_getpgrp(trap_frame_t *regs)
{
	return kgetpgrp();
}
int sys_setpgid(trap_frame_t *regs)
{
	int pid = regs->ebx;
	int pgid = regs->ecx;

	if (pid < 0 || pgid < 0)
		return -EINVAL;

	return ksetpgid(pid, pgid);
}
/*
 * the same as setpgid
 */
int sys_setpgrp(trap_frame_t *regs)
{
	int pid = regs->ebx;
	int pgid = regs->ecx;

	if (pid < 0 || pgid < 0)
		return -EINVAL;
	return ksetpgid(pid, pgid);
}

int sys_isatty(trap_frame_t *regs)
{
	return kisatty(regs->ecx);
}


int sys_kill(trap_frame_t *regs)
{
	int pid = regs->ebx;
	if (pid < 0)
		return -EINVAL;

	int status = regs->ecx;
	return kterminate_proc(pid, status);
}
/*
 * create a hard link
 * this function is wired,
 * it should be discarded
 */
int sys_link(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_lseek(trap_frame_t *regs)
{
	int fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;

	off_t offset = regs->ecx;
	int whence = regs->edx;
	if (whence != SEEK_SET && whence != SEEK_CUR
			&& whence != SEEK_END)
		return -EINVAL;

	return klseek(fd, offset, whence);
}

int sys_stat(trap_frame_t *regs)
{
	char *up = (char *)regs->ebx;
	struct stat st;
	int ret;
	struct stat *buf = (struct stat *)regs->ecx;
	if (up == NULL || buf == NULL)
		return -EINVAL;

	char *path = vfs_get_name(up);
	if (!path)
		return -EFAULT;

	memset((char *)&st, 0, sizeof(st));

	ret = kstat(path, &st);
	if (ret < 0){
		goto end;
	}

	ret = copy_to_user((char *)buf, (char *)&st, sizeof(st));
	if (ret < 0)
		goto end;
	ret = 0;
end:
	vfs_put_name(path);
	return ret;

}

int sys_times(trap_frame_t *regs)
{
	struct tms *tms = (struct tms *)regs->ecx;
	return ktimes(tms);
}

int sys_unlink(trap_frame_t *regs)
{
	char *name = (char *)regs->ecx;
	if (!name)
		return -EINVAL;
	char *path = vfs_get_name(name);
	if (!path)
		return -EFAULT;
	return kunlink(path);
}

int sys_gettimeofday(trap_frame_t *regs)
{
	struct timeval *tv = (struct timeval *)regs->ebx;
	if (!tv)
		return -EINVAL;
	int *tz = (int *)regs->ecx;
	return kgettimeofday(tv, tz);

}

int sys_socket(trap_frame_t *regs)
{
	int dom, type, proto;

	dom = regs->ebx;
	type = regs->ecx;
	proto = regs->edx;

	return ksocket(dom, type, proto);
}

int sys_bind(trap_frame_t *regs)
{
	int fd;
	struct sockaddr *uaddr;
	struct sockaddr kaddr;
	socklen_t addrlen;
	int retval;

	fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;
	uaddr = (struct sockaddr *)regs->ecx;
	if (uaddr == NULL)
		return -EINVAL;
	addrlen = regs->edx;

	if (addrlen > sizeof(struct sockaddr))
		return -ENAMETOOLONG;
	memset((void *)&kaddr, 0, sizeof(struct sockaddr));

	retval = copy_from_user((char *)&kaddr, (char *)uaddr, addrlen);
	if (retval < 0)
		return -EFAULT;

	return kbind(fd, &kaddr, addrlen);
}

int sys_listen(trap_frame_t *regs)
{
	int fd, backlog;

	fd = regs->ebx;
	backlog = regs->ecx;

	if (fd < 0)
		return -EINVAL;
	return klisten(fd, backlog);
}

int sys_accept(trap_frame_t *regs)
{
	int fd, newfd;
	struct sockaddr *uaddr;
	struct sockaddr kaddr;
	socklen_t *usocklen;
	socklen_t ksocklen;
	int retval;

	fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;

	uaddr = (struct sockaddr *)regs->ecx;
	usocklen = (socklen_t *)regs->edx;

	if (uaddr != NULL) {
		retval = copy_from_user((char *)&ksocklen,
				(char *)usocklen, sizeof(socklen_t));
		if (retval < 0)
			goto end;
	}
	memset(&kaddr, 0, sizeof(kaddr));
	retval = kaccept(fd, &kaddr, &ksocklen);
	if (retval < 0)
		goto end;

	newfd = retval;

	if (uaddr != NULL) {
		retval = copy_to_user((caddr_t)uaddr, (caddr_t)&kaddr, ksocklen);
		if (retval < 0)
			goto err;

		if (usocklen != NULL) {
			retval = copy_to_user((caddr_t)usocklen, (caddr_t)&ksocklen, sizeof(socklen_t));
			if (retval < 0)
				goto err;
		}
	}
	retval = newfd;
err:
	kclose(newfd);
end:
	return retval;
}

int sys_sendto(trap_frame_t *regs)
{
	struct sendto_args *parg;
	struct sendto_args karg = {0};
	int retval;
	struct msghdr msg;
	struct iovec aiov;

	parg = (struct sendto_args *)regs->ecx;
	if (parg == NULL)
		return -EINVAL;

	retval = copy_from_user((caddr_t)&karg, (caddr_t)parg, sizeof(karg));
	if (retval < 0)
		goto end;

	if (karg.tolen > sizeof(struct sockaddr))
		return -ENAMETOOLONG;

	msg.msg_name = karg.to;
	msg.msg_namelen = karg.tolen;

	msg.msg_iov = &aiov;
	msg.msg_control = 0;
	msg.msg_iovlen = 1;

	aiov.iov_base = karg.buf;
	aiov.iov_len = karg.len;

	retval = ksendit(karg.fd, &msg, karg.flags);

end:
	return retval;
}

int sys_sendmsg(trap_frame_t *regs)
{
	int fd, retval;
	struct msghdr *msg, kmsg;
	int flags;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;

	msg = (struct msghdr *)regs->ecx;
	if (msg == NULL)
		return -EINVAL;

	flags = regs->edx;

	memset(&kmsg, 0, sizeof(kmsg));

	retval = copy_from_user((caddr_t)&kmsg, (caddr_t)msg, sizeof(kmsg));
	if (retval < 0)
		goto end;

	if (kmsg.msg_iovlen >= UIO_SMALLIOV) {
		if (kmsg.msg_iovlen >= UIO_MAXIOV)
			return -EMSGSIZE;

		iov = (struct iovec *)kmalloc(sizeof(struct iovec) * kmsg.msg_iovlen);
	} else
		iov = aiov;

	retval = -EINVAL;
	if (kmsg.msg_iovlen && (retval = copy_from_user((caddr_t)iov, (caddr_t)kmsg.msg_iov,
			kmsg.msg_iovlen * sizeof(struct iovec))) < 0)
		goto end;

	kmsg.msg_iov = iov;

	retval = ksendit(fd, &kmsg, flags);

end:
	if (iov != aiov)
		kfree(iov);
	return retval;
}

int sys_recvfrom(trap_frame_t *regs)
{
	struct recvfrom_args *pargs, kargs;
	struct msghdr msg;
	struct iovec aiov;
	int retval;

	pargs = (struct recvfrom_args *)regs->ecx;
	retval = copy_from_user((caddr_t)&kargs, (caddr_t)pargs, sizeof(kargs));
	if (retval < 0)
		return retval;

	if (kargs.fromlen) {
		retval = copy_from_user((caddr_t)&msg.msg_namelen,
				(caddr_t)kargs.fromlen, sizeof(msg.msg_namelen));
		if (retval < 0)
			goto end;
	} else
		msg.msg_namelen = 0;

	msg.msg_name = kargs.from;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	msg.msg_flags = kargs.flags;
	aiov.iov_base = kargs.buf;
	aiov.iov_len = kargs.len;

	retval = krecvit(kargs.fd, &msg, kargs.fromlen);
	if (retval < 0)
		goto end;
//	size = retval;
//
//	if (kargs.fromlen) {
//		retval = copy_to_user((caddr_t)pargs->fromlen, (caddr_t)kargs.fromlen, sizeof(kargs.fromlen));
//		if (retval < 0)
//			goto end;
//	}
//	retval = size;
end:
	return retval;
}

int sys_recvmsg(trap_frame_t *regs)
{
	struct msghdr msg, *pmsg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	int retval;
	int fd, flags, size;

	fd = regs->ebx;
	if (fd < 0)
		return -EINVAL;
	flags = regs->edx;

	pmsg = (struct msghdr *)regs->ecx;
	if (pmsg == NULL)
		return -EINVAL;

	retval = copy_from_user((caddr_t)&msg, (caddr_t)pmsg, sizeof(msg));
	if (retval < 0)
		return retval;

	if (msg.msg_iovlen >= UIO_SMALLIOV) {
		if(msg.msg_iovlen >= UIO_MAXIOV)
			return -EMSGSIZE;

		iov = (struct iovec *)kmalloc(sizeof(struct iovec) * msg.msg_iovlen);
		if (iov == NULL)
			return -ENOBUFS;
	} else
		iov = aiov;

	msg.msg_flags = flags;
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	retval = copy_from_user((caddr_t)iov, (caddr_t)uiov,
			sizeof(struct iovec) * msg.msg_iovlen);

	if (retval < 0)
		goto end;

	retval = krecvit(fd, &msg, NULL);
	if (retval < 0)
		goto end;

	size = retval;
	msg.msg_iov = uiov;
	retval = copy_to_user((caddr_t)pmsg, (caddr_t)&msg, sizeof(msg));
	if (retval < 0)
		goto end;

	retval = size;
end:
	if (iov != aiov)
		kfree(iov);
	return retval;
}

int sys_shutdown(trap_frame_t *regs)
{
	int fd;
	int how;

	fd = regs->ebx;
	how = regs->ecx;

	return kshutdown(fd, how);
}

int sys_ioctl(trap_frame_t *regs)
{
	int fd, retval;
	caddr_t data, buf, memp;
	int size;
	u32 cmd;
	u8 stackbuf[128] = {0};

	LOG_INFO("in");
	fd = regs->ebx;
	if (fd < 0) {
		LOG_ERROR("fd:%d", fd);
		return -EINVAL;
	}


	cmd = regs->ecx;
	size = IOCPARM_LEN(cmd);
	//LOG_INFO("size:%d", size);
	if (size > IOCPARM_MAX) {
		LOG_ERROR("size:%d", size);
		return -ENOTTY;
	}
	buf = (caddr_t)regs->edx;

	if (size > sizeof(stackbuf)) {
		data = kzalloc(size);
		memp = data;
	} else {
		memp = NULL;
		data = (caddr_t)stackbuf;
	}
	//LOG_INFO("name:%s", buf);
	if (cmd & IOC_IN) {
		if (size) {
			retval = copy_from_user(data, buf, size);
			if (retval < 0) {
				kfree(memp);
				LOG_ERROR("retval:%d", retval);
				return retval;
			}
		} else
			*(caddr_t *)data = buf;
	} else if (cmd & IOC_VOID)
		*(caddr_t *)data = buf;

	//LOG_INFO("ifname:%s", data);

	retval = kioctl(fd, cmd, data);
	if (retval == 0 && (cmd & IOC_OUT) && size) {
		retval = copy_to_user(buf, data, size);
	}

	if (memp)
		kfree(memp);
	LOG_INFO("end.ret:%d", retval);
	return retval;
}


int sys_fcntl(trap_frame_t *regs)
{
	int fd = regs->ebx;
	if (fd < 0 || fd > 1024)
		return -EINVAL;
	int cmd = regs->ecx;

	int arg = regs->edx;

	return kfcntl(fd, cmd, arg);
}


int sys_getdents(trap_frame_t *regs)
{
	int fd, size;
	int retval;
	u8 *ubuf, *kbuf;
	fd= regs->ebx;
	if (fd < 0)
		return -EINVAL;
	ubuf = (u8 *)regs->ecx;
	if (ubuf == NULL)
		return -EINVAL;
	size = regs->edx;
	if (size < 0)
		return -EINVAL;

	kbuf = kzalloc(size);
	//assert(kbuf != NULL);
	if (kbuf == NULL)
		return -ENOMEM;

	retval = kgetdents(fd, kbuf, size);
	if (retval < 0)
		goto end;

	retval = copy_to_user((char *)ubuf, (char *)kbuf, retval);
end:
	//LOG_INFO("end:%d", retval);
	kfree(kbuf);
	return retval;
}

int sys_setloglevel(trap_frame_t *regs)
{
	extern u32 curr_log_level;

	int level = regs->ecx;
	if (level > KLOG_LEVEL_NONE || level < KLOG_LEVEL_DEBUG)
		return -EINVAL;
	LOG_NOTE("set log level to :%d", level);
	curr_log_level = level;
	return 0;
}

int sys_dup2(trap_frame_t *regs)
{
	int newfd = regs->ecx;
	int oldfd = regs->ebx;

	if (oldfd < 0 || newfd < 0)
		return -EINVAL;
	if (newfd == oldfd)
		return 0;
	return kdup2(oldfd, newfd);
}

int sys_chdir(trap_frame_t *regs)
{
	int ret;
	s8 *up = (s8 *)regs->ebx;
	if (up == NULL)
		return -EINVAL;
	char *path = vfs_get_name(up);
	if (path == NULL)
		return -EFAULT;

	ret = kchdir(path);
	vfs_put_name(path);
	return ret;
}

int sys_fchdir(trap_frame_t *regs)
{
	int fd = regs->ebx;
	if (fd < 0 || fd > 1024	)
		return -EBADF;

	return kfchdir(fd);
}

int sys_mkdir(trap_frame_t *regs)
{
	char *uname = (char *)regs->ebx;
	int mode = regs->ecx;
	int ret;
	char *path = vfs_get_name(uname);
	if (path == NULL)
		return -EINVAL;
	ret = kmkdir(path, mode);
	vfs_put_name(path);
	return ret;
}


int sys_tcgetpgrp(trap_frame_t *regs)
{
	int fd;

	fd = regs->ecx;
	if (fd < 0)
		return -EINVAL;

	return ktcgetpgrp(fd);
}

int sys_tcsetpgrp(trap_frame_t *regs)
{
	return 0;
}

int sys_frame_create(trap_frame_t *regs)
{
	rect_t rect;
	int flag;
	int ret;
	rect_t *prct;
	int fm_id;

	fm_id = regs->ebx;
	if (fm_id < 0 || fm_id > 1024)
		return -EINVAL;


	prct = (rect_t *)regs->ecx;
	if (prct == NULL)
		return -EINVAL;


	if (copy_from_user((char *)&rect, (char *)prct, sizeof(rect)) < 0) {
		ret = -EFAULT;
		goto end;
	}

	flag = regs->edx;

	ret = kframe_create(fm_id, &rect, flag);
end:
	return ret;
}


int sys_window_create(trap_frame_t *regs)
{
	char *str;
	rect_t rect;
	int flag;
	int ret;

	str = kzalloc(16);
	if (str == NULL)
		return -ENOBUFS;

	if (regs->ebx != 0) {
		if (copy_from_user(str, (char *)regs->ebx, 16) < 0) {
			ret = -EFAULT;
			goto end;
		}
	} else {
		kfree(str);
		str = NULL;
	}



	if (copy_from_user((char *)&rect, (char *)regs->ecx, sizeof(rect)) < 0) {
		ret = -EFAULT;
		goto end;
	}

	flag = regs->edx;

	ret = kwindow_create(str, &rect, flag);
end:
	if (str != NULL)
		kfree(str);
	return ret;
}


int sys_get_message(trap_frame_t *regs)
{
	int ret;
	MSG msg;
	MSG *pmsg;


	if (regs->ebx == 0)
		return -EINVAL;

	pmsg = (MSG *)regs->ebx;


	ret = kframe_get_message(&msg);


	if (copy_to_user((char *)pmsg, (char *)&msg, sizeof(MSG)) < 0)
		return -EFAULT;

	return ret;
}

int sys_frame_text(trap_frame_t *regs)
{
	char *str;
	int len;
	int ret;
	int fm_id;

	if (regs->ecx == 0)
		return -EINVAL;

	len = strnlen_user((char *)regs->ecx, 32)+1;
	if (len < 0)
		return -EFAULT;

	str = kzalloc(len);
	if (str == NULL)
		return -ENOBUFS;

	if (copy_from_user(str, (char *)regs->ecx, len) < 0) {
		ret = -EFAULT;
		goto end;
	}
	fm_id = regs->ebx;
	if (fm_id <0 || fm_id > 1024) {
		ret = -EINVAL;
		goto end;
	}

	ret = kframe_text(fm_id, str);

end:
	if (str != NULL)
		kfree(str);
	return ret;
}


int sys_frame_draw_char(trap_frame_t *regs)
{
	int fm_id;
	int c;
	int ret;
	rect_t rect;
	rect_t *prct;

	fm_id = regs->ebx;
	if ( fm_id < 1024) {
		ret = -EINVAL;
		goto end;
	}

	prct = (rect_t *)regs->ecx;
	if (prct == NULL)
		return -EINVAL;


	if (copy_from_user((char *)&rect, (char *)prct, sizeof(rect_t)) < 0)
		return -EFAULT;

	c = regs->edx;

	ret = kframe_draw_char(fm_id, &rect, c);

	end:
	return ret;
}

int sys_frame_draw_string(trap_frame_t *regs)
{
	int fm_id;
	int ret;
	rect_t rect;
	rect_t *prct;
	char *str;
	int len;

	fm_id = regs->ebx;
	if ( fm_id < 1024) {
		ret = -EINVAL;
		goto end;
	}

	prct = (rect_t *)regs->ecx;
	if (prct == NULL)
		return -EINVAL;


	if (copy_from_user((char *)&rect, (char *)prct, sizeof(rect_t)) < 0)
		return -EFAULT;

	len = strnlen_user((char *)regs->edx, 32)+1;
	if (len < 0)
		return -EFAULT;

	str = kzalloc(len);
	if (str == NULL)
		return -ENOBUFS;

	if (copy_from_user(str, (char *)regs->edx, len) < 0) {
		ret = -EFAULT;
		goto end;
	}

	ret = kframe_draw_string(fm_id, &rect, str);

	end:
	if (str != NULL)
		kfree(str);
	return ret;
}


int sys_frame_show(trap_frame_t *regs)
{
	int fm_id;

	fm_id = regs->ebx;
	if (fm_id < 0 || fm_id > 1024)
		return -EINVAL;

	return kframe_show(fm_id);
}



int sys_resolve(trap_frame_t *regs)
{
	int reloc_entry;
	reloc_entry = regs->ecx;
	void *em = (void *)regs->edx;

	return kelf_resolv(em, reloc_entry);
}

int sys_connect(trap_frame_t *regs)
{
	struct connect_args arg;
	struct sockaddr addr;

	if (copy_from_user((char *)&arg, (char *)regs->ecx, sizeof(arg)) < 0)
		return -EFAULT;

	if (arg.len != sizeof(addr))
		return -EINVAL;

	if (arg.fd < 0 || arg.fd >= 1024)
		return -EINVAL;

	if (copy_from_user((char *)&addr, (char *)arg.buf, arg.len) < 0)
		return -EFAULT;

	return kconnect(arg.fd, &addr, arg.len);
}

int sys_getpriority(trap_frame_t *regs)
{
	int which;
	id_t who;
	which = (int)regs->ebx;
	who = (id_t)regs->ecx;

	return kgetpriority(which, who);

}

int sys_setpriority(trap_frame_t *regs)
{
	int which, value;
	id_t who;

	which = (int)regs->ebx;
	who = (id_t)regs->ecx;
	value = (int)regs->edx;

	return ksetpriority(which, who, value);

}

int sys_getrusage(trap_frame_t *regs)
{
	int who;
	struct rusage *rs;
	struct rusage krs;
	int ret;
	memset(&krs, 0, sizeof(krs));

	who = regs->ebx;
	rs = (struct rusage *)regs->ecx;

	ret = kgetrusage(who, &krs);
	if (ret < 0)
		return ret;

	ret = copy_to_user((char *)rs, (char *)&krs, sizeof(krs));
	if (ret < 0)
		return -EFAULT;
	return 0;
}

int sys_setitimer(trap_frame_t *regs)
{

	int ret;
	int which;
	struct itimerval val, oval;
	struct itimerval *oldval, *uval;

	which = regs->ebx;
	uval = (struct itimerval *)regs->ecx;
	if (!uval)
		return -EINVAL;
	oldval = (struct itimerval *)regs->edx;

	if (copy_from_user((char *)&val, (char *)uval, sizeof(val)) < 0)
		return -EFAULT;


	ret = ksetitimer(which, &val, oldval == NULL ? NULL : &oval);
	if (ret < 0)
		return ret;
	if (oldval){
		if (copy_to_user((char *)oldval, (char *)&oval, sizeof(oval)) < 0)
			return -EFAULT;		// unset timer?
	}
	return ret;
}

int sys_getitimer(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_vfork(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_select(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_getlogin(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_socketpair(trap_frame_t *regs)
{
	int ret;
	struct socketpair_args args, *parg;
	int retval[2];
	parg = (struct socketpair_args *)regs->ecx;
	if (!parg)
		return -EINVAL;

	ret = copy_from_user((char *)&args, (char *)parg, sizeof(args));
	if (ret < 0)
		return -EFAULT;

	ret = ksocketpair(args.domain, args.protocol, args.type, retval);
	if (ret < 0)
		return ret;
	if (args.sv) {
		ret = copy_to_user((char *)args.sv, (char *)retval, sizeof(retval));
		if (ret < 0){
			ret = -EFAULT;
			goto err;
		}
		goto end;
	}
	goto err;
end:
	return ret;
err:
	kclose(retval[0]);
	kclose(retval[1]);
	goto end;
}


int sys_getsockopt(trap_frame_t *regs)
{
	int ret;
	struct sockopt_arg {
		int sockfd;
		int level;
		int optname;
		void *optval;
		socklen_t *optlen;
	};
	struct sockopt_arg *puarg, args;
	void *optval = NULL;
	socklen_t optlen = 0;
	puarg = (struct sockopt_arg *)regs->ebx;
	if (!puarg)
		return -EINVAL;

	if (copy_from_user((char *)&args, (char *)puarg, sizeof(args)) < 0)
		return -EFAULT;

	if (puarg->optlen) {
		if (copy_from_user((char *)&optlen, (char *)puarg->optlen, sizeof(socklen_t)) < 0)
			return -EFAULT;
		optval = kzalloc(optlen);
		if (!optval)
			return -ENOBUFS;
	}

	ret = kgetsockopt(args.sockfd, args.level, args.optname, optval, &optlen);
	if (ret < 0)
		goto end;

	if (puarg->optval) {
		ret = copy_to_user((char *)puarg->optval, (char *)optval, optlen);
		if (ret < 0){
			ret = -EFAULT;
			goto end;
		}
	}
	if (puarg->optlen){
		ret = copy_to_user((char *)puarg->optlen, (char *)&optlen, sizeof(socklen_t));
		if (ret < 0) {
			ret = -EFAULT;
			goto end;
		}
	}
	ret = 0;
end:
	if (optval)
		kfree(optval);
	return ret;

}

int sys_setsockopt(trap_frame_t *regs)
{
	int ret;
	struct sockopt_arg {
		int sockfd;
		int level;
		int optname;
		void *optval;
		socklen_t optlen;
	};
	struct sockopt_arg *puarg, args;
	void *optval = NULL;

	puarg = (struct sockopt_arg *)regs->ebx;
	if (!puarg)
		return -EINVAL;

	if (copy_from_user((char *)&args, (char *)puarg, sizeof(args)) < 0)
		return -EFAULT;

	if (args.optval) {
		optval = kzalloc(args.optlen);
		if (!optval)
			return -ENOBUFS;
		if (copy_from_user((char *)optval, (char *)args.optval, args.optlen) < 0) {
			ret = -EFAULT;
			goto end;
		}
	}

	ret = ksetsockopt(args.sockfd, args.level, args.optname, optval, args.optlen);
	if (ret < 0)
		goto end;

	if (args.optval) {
		ret = copy_to_user((char *)args.optval, (char *)optval, args.optlen);
		if (ret < 0){
			ret = -EFAULT;
			goto end;
		}
	}

	ret = 0;
end:
	if (optval)
		kfree(optval);
	return ret;
}

int sys_seteuid(trap_frame_t *regs)
{

	int euid = (int)regs->ecx;
	return kseteuid(euid);
}
int sys_setegid(trap_frame_t *regs)
{
	int egid = (int)regs->ecx;
	return ksetegid(egid);
}
int sys_getdirentries(trap_frame_t *regs)
{
	int ret;
	struct getdirentries_args {
		int fd;
		char *buf;
		int nbytes;
		long *basep;
	};
	struct getdirentries_args *parg, args;
	char *buf;
	long basep = 0;
	int nread;
	parg = (struct getdirentries_args *)regs->ecx;
	ret = copy_from_user((char *)&args, (char *)parg, sizeof(args));
	if (ret < 0)
		return -EFAULT;
	buf = kmalloc(args.nbytes);
	if (!buf)
		return -ENOBUFS;

	ret = copy_from_user((char *)&basep, (char *)args.basep, sizeof(basep));
	if (ret < 0)
		goto end;

	ret = kgetdirentries(args.fd, buf, args.nbytes, basep);
	if (ret < 0)
		goto end;
	nread = ret;

	ret = copy_to_user(args.buf, buf, args.nbytes);
	if (ret < 0)
		goto end;

	ret = copy_to_user((char *)args.basep, (char *)&basep, sizeof(basep));
	if (ret < 0)
		goto end;

	ret = nread;
end:
	if (buf)
		kfree(buf);
	return ret;
}

int sys_fstatfs(trap_frame_t *regs)
{
	int ret;
	int fd;
	struct statfs sf;
	struct statfs *usf;

	fd = (int)regs->ebx;
	if(fd < 0 || fd > 1024)
		return -EINVAL;

	usf = (struct statfs *)regs->ecx;
	if (!usf)
		return -EINVAL;

	memset((char *)&sf, 0, sizeof(sf));

	ret = kfstatfs(fd, &sf);
	if (ret < 0)
		return ret;

	ret = copy_to_user((char *)usf, (char *)&sf, sizeof(sf));
	if (ret < 0)
		return -EFAULT;

	return 0;
}
int sys_statfs(trap_frame_t *regs)
{
	int ret;

	struct statfs *sf = NULL;
	struct statfs *usf;
	char *path = NULL;
	char *upath = (char *)regs->ebx;
	if (!upath)
		return -EINVAL;

	ret = -EINVAL;
	usf = (struct statfs *)regs->ecx;
	if (!usf)
		goto end;

	ret = -ENOMEM;
	sf = kzalloc(sizeof(struct statfs));
	if (sf == NULL)
		goto end;

	ret = -EFAULT;
	path = vfs_get_name(upath);
	if (path == NULL)
		goto end;

	ret = kstatfs(path, sf);
	if (ret < 0)
		goto end;

	ret = -EFAULT;
	ret = copy_to_user((char *)usf, (char *)sf, sizeof(struct statfs));
	if (ret < 0)
		goto end;

	ret = 0;

end:
	if (path)
		vfs_put_name(path);
	if (sf)
		kfree(sf);
	return ret;
}

int sys_chmod(trap_frame_t *regs)
{
	char *path;
	mode_t mode;
	int ret;
	char *pupath = (char *)regs->ebx;
	if (!pupath)
		return -EINVAL;

	path = vfs_get_name(pupath);
	if (!path)
		return -EFAULT;
	mode = (mode_t)regs->ecx;
	ret = kchmod(path, mode);
	vfs_put_name(path);
	return ret;
}

int sys_fchmod(trap_frame_t *regs)
{

	int fd;
	mode_t mode;
	fd = (int)regs->ebx;
	if (fd < 0 || fd > 1024)
		return -EINVAL;

	mode = (mode_t)regs->ecx;

	return kfchmod(fd, mode);
}

int sys_mmap(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_munmap(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_utimes(trap_frame_t *regs)
{
	int ret;
	char *upath;
	char *path = NULL;
	struct timeval times[2];
	struct timeval *pt;
	upath = (char *)regs->ebx;
	pt = (struct timeval *)regs->ecx;

	if (!upath)
		return -EINVAL;

	path = vfs_get_name(upath);
	if (path == NULL)
		return -EFAULT;

	memset((char *)&times, 0, sizeof(times));

	if (pt){
		ret = copy_from_user((char *)&times,  (char *)pt, sizeof(times));
		if (ret < 0)
			goto end;
	}

	ret = kutimes(path, pt ? times : NULL);

end:
	if (path)
		vfs_put_name(path);
	return ret;
}

int sys_wait4(trap_frame_t *regs)
{
	return -ENOSYS;
}

int sys_getrlimit(trap_frame_t *regs)
{
	int resource;
	struct rlimit rlimt = {0};
	struct rlimit *plimt;
	int ret;

	resource = (int)regs->ebx;
	plimt  = (struct rlimit *)regs->ecx;

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return -EINVAL;

	if (plimt == NULL)
		return -EINVAL;

	ret = kgetrlimit(resource, &rlimt);
	if (ret < 0)
		return ret;

	ret = copy_to_user((char *)plimt, (char *)&rlimt, sizeof(rlimt));
	if (ret < 0)
		return -EFAULT;

	return 0;
}
int sys_setrlimit(trap_frame_t *regs)
{
	int resource;
	struct rlimit rlimt = {0};
	struct rlimit *plimt;
	int ret;

	resource = (int)regs->ebx;
	plimt  = (struct rlimit *)regs->ecx;

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return -EINVAL;

	if (plimt == NULL)
		return -EINVAL;

	ret = copy_from_user((char *)&rlimt, (char *)plimt,  sizeof(rlimt));
	if (ret < 0)
		return -EFAULT;

	return ksetrlimit(resource, &rlimt);
}
int sys_ftruncate(trap_frame_t *regs)
{
	int fd;
	off_t len;

	fd = (int)regs->ebx;
	if (fd < 0 || fd > 1024)
		return -EINVAL;

	len = (off_t)regs->ecx;

	return kftruncate(fd, len);
}
int sys_fsync(trap_frame_t *regs)
{
	int fd;

	fd = (int)regs->ecx;
	if (fd < 0 || fd > 1024)
		return -EINVAL;

	return kfsync(fd);
}

int sys_settimeofday(trap_frame_t *regs)
{
	int ret;
	struct timeval *ptv;
	struct timeval tv;
	struct timezone *ptz;
	struct timezone tz;

	ptv = (struct timeval *)regs->ebx;
	if (ptv) {
		ret = copy_from_user((char *)&tv, (char *)ptv, sizeof(struct timeval));
		if (ret < 0)
			return -EFAULT;
	}

	ptz = (struct timezone *)regs->ecx;
	if (ptz) {
		ret = copy_from_user((char *)&tz, (char *)ptz, sizeof(struct timezone));
		if (ret < 0)
			return -EFAULT;
	}

	return ksettimeofday(ptv ? &tv : NULL, ptz ? &tz : NULL);
}

const syscall syscall_table[] = {
		sys_exit,
		sys_open,
		sys_read,
		sys_write,
		sys_unlink,
		sys_close,
		sys_sleep,
		sys_wait,
		sys_sbrk,
		sys_fstat,
		sys_fork,
		sys_getpid,
		sys_getppid,
		sys_isatty,
		sys_kill,
		sys_lseek,
		sys_stat,
		sys_times,
		sys_execve,
		sys_gettimeofday,
		sys_socket,
		sys_bind,
		sys_listen,
		sys_accept,
		sys_sendto,
		sys_sendmsg,
		sys_recvfrom,
		sys_recvmsg,
		sys_shutdown,
		sys_ioctl,
		sys_fcntl,
		sys_getdents,
		sys_setloglevel,
		sys_dup2,
		sys_chdir,
		sys_fchdir,
		sys_getuid,
		sys_geteuid,
		sys_getgid,
		sys_getegid,
		sys_setpgid,
		sys_getpgid,
		sys_getpgrp,
		sys_setpgrp,
		sys_tcgetpgrp,
		sys_tcsetpgrp,
		sys_mkdir,
		sys_frame_create,
		sys_frame_text,
		sys_frame_show,
		sys_resolve,
		sys_connect,
		sys_readv,
		sys_writev,
		sys_getpriority,
		sys_setpriority,
		sys_getrusage,
		sys_setitimer,
		sys_getitimer,
		sys_vfork,
		sys_select,
		sys_getlogin,
		sys_socketpair,
		sys_seteuid,
		sys_setegid,
		sys_getdirentries,
		sys_fstatfs,
		sys_statfs,
		sys_mmap,
		sys_munmap,
		sys_utimes,
		sys_wait4,
		sys_getrlimit,
		sys_setrlimit,
		sys_ftruncate,
		sys_fsync,
		sys_settimeofday,
		sys_getsockopt,
		sys_setsockopt,
		sys_chmod,
		sys_fchmod,
		sys_window_create,
		sys_get_message,
		sys_frame_draw_char,
		sys_frame_draw_string
};


const int nr_syscalls = sizeof(syscall_table) / sizeof(syscall);
