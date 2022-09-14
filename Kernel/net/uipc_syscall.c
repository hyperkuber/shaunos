/*
 * uipc_syscall.c
 *
 *  Created on: Oct 23, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/types.h>
#include <kernel/net/uipc_syscall.h>
#include <kernel/kthread.h>
#include <kernel/vfs.h>
#include <kernel/mbuf.h>
#include <kernel/uio.h>
#include <string.h>
#include <kernel/sys/socketval.h>
#include <kernel/stat.h>
#include <kernel/sys/protosw.h>
#include <kernel/sys/sockio.h>
#include <kernel/net/if.h>

struct file_ops sockops;

extern char netcon[];
int
ksocket(int dom, int type, int proto)
{
	int retval;
	file_t *fp;
	kprocess_t *proc;
	int fd;
	struct socket *so;

	proc = current->host;
	fp = vfs_alloc_file();
	if (fp == NULL) {
		LOG_ERROR("no buf");
		return -ENOBUFS;
	}


	fp->f_type |= (S_IFSOCK | S_IRWXU);
	fp->f_flags |= (FREAD | FWRITE);
	fp->f_ops = &sockops;

	fp->f_refcnt++;

	retval = socreate(dom, &so, type, proto);
	if (retval < 0) {
		LOG_ERROR("socreate failed");
		goto end;
	}


	fp->f_vnode = (struct vnode *)so;

	fd = get_unused_id(&proc->p_fd_obj);
	proc->open_files++;
	//LOG_INFO("end.fd:%d", fd);
	return fd_attach(fd, proc, fp);
end:
	vfs_free_file(fp);
	return retval;
}

int
kbind(int fd, struct sockaddr *addr, socklen_t len)
{
	kprocess_t *proc = current->host;

	file_t *fp;
	struct mbuf *nam;
	int retval;

	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;
	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	nam = m_get(M_NOWAIT, MT_DATA);
	if (nam == NULL)
		return -ENOBUFS;

	memcpy((void *)mtod(nam, caddr_t), (void *)addr, len);

	retval = sobind((struct socket *)fp->f_vnode, nam);
	m_freem(nam);
	return retval;
}

int
klisten(int fd, int backlog)
{
	kprocess_t *proc = current->host;
	file_t *fp;

	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;
	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	return solisten((struct socket *)fp->f_vnode, backlog);;
}

int
kaccept(int fd, struct sockaddr *addr, socklen_t *len)
{
	kprocess_t *proc = current->host;
	file_t *fp, *newfp;
	struct socket *so, *aso;
	fp = (file_t *)proc->file[fd];
	int retval, newfd;
	struct mbuf *nam;

	if (fp == NULL)
		return -EBADF;
	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	so = (struct socket *)fp->f_vnode;
	if ((so->so_options & SO_ACCEPTCONN) == 0)
		return -EINVAL;

	if((so->so_state & SS_NBIO) && so->so_qlen == 0)
		return -EWOULDBLOCK;

	while (so->so_qlen == 0 && so->so_error == 0) {
		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = -ECONNABORTED;
			break;
		}

		retval = sbsleep(&so->timoq, netcon, so->so_timeo);
		if (retval < 0)
			return retval;
	}

	if (so->so_error) {
		retval = so->so_error;
		so->so_error = 0;
		return retval;
	}

	newfp = vfs_alloc_file();
	if (newfp == NULL)
		return -ENOBUFS;

	aso = so->so_q;
	if (soqremque(aso, 1) == 0)
		LOG_WARN("accept queue");
	so = aso;

	newfp->f_type |= (S_IFSOCK | S_IRWXU);
	newfp->f_flags = FREAD|FWRITE;
	newfp->f_ops = &sockops;
	newfp->f_vnode = (struct vnode *)so;

	nam = m_get(M_DONTWAIT, MT_DATA);
	soaccept(so, nam);

	if (addr) {
		if (*len > nam->m_len)
			*len = nam->m_len;
		memcpy((void *)addr, (void *)(mtod(nam, caddr_t)), *len);
	}

	m_freem(nam);

	newfd = get_unused_id(&proc->p_fd_obj);

	retval = fd_attach(newfd, proc, newfp);
	if (retval < 0)
		return retval;

	return newfd;
}

int
kconnect(int fd, struct sockaddr *addr, socklen_t len)
{
	file_t *fp;
	kprocess_t *proc = current->host;
	struct socket *so;
	int ret;
	struct mbuf *nam;

	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;
	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	so = (struct socket *)fp->f_vnode;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
			return -EALREADY;

	nam = m_get(M_DONTWAIT, MT_DATA);
	if (nam == NULL)
		return -ENOBUFS;

	addr->sa_len = len;
	memcpy(mtod(nam, caddr_t), (caddr_t)addr, len);

	ret = soconnect(so,nam);
	if (ret < 0)
		goto end;

	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)){
		m_freem(nam);
		return -EINPROGRESS;
	}

	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
			if ((ret = sbsleep(&so->timoq, netcon, so->so_timeo)) < 0)
				break;
	if (ret == 0) {
		ret = so->so_error;
		so->so_error = 0;
	}
end:
	so->so_state &= ~SS_ISCONNECTING;
	if(nam)
		m_freem(nam);
	return ret;
}


int
ksendit(int fd, struct msghdr *msg, int flags)
{
	file_t *fp;
	struct uio auio;
	struct iovec *iov;
	int i;
	struct mbuf *to, *control;
	int len, retval;
	kprocess_t *proc = current->host;

	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;

	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;
	auio.uio_iov = msg->msg_iov;
	auio.uio_iovcnt = msg->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = 0;
	auio.uio_resid = 0;

	iov = msg->msg_iov;

	for (i=0; i<msg->msg_iovlen; i++, iov++) {
		if (auio.uio_resid + iov->iov_len < auio.uio_resid)
			return -EINVAL;
		auio.uio_resid += iov->iov_len;
	}

	if (msg->msg_namelen > MLEN) {
		return -EINVAL;
	}

	if (msg->msg_name) {
		to = m_get(M_DONTWAIT, MT_DATA);
		if (to == NULL)
			return -ENOBUFS;

		retval = copy_from_user(mtod(to, caddr_t), (caddr_t)msg->msg_name, msg->msg_namelen);
		if (retval < 0) {
			m_freem(to);
			return retval;
		}

		to->m_len = msg->msg_namelen;

	} else
		to = NULL;



	if (msg->msg_control) {
		if (msg->msg_controllen < sizeof(struct msghdr) || msg->msg_controllen > MLEN) {
			if (to)
				m_freem(to);
			return -EINVAL;
		}

		control = m_get(M_DONTWAIT, MT_DATA);
		if (control == NULL) {
			if (to)
				m_freem(to);
			return -ENOBUFS;
		}

		retval = copy_from_user(mtod(control, caddr_t), (caddr_t)msg->msg_control, msg->msg_controllen);
		if (retval < 0) {
			if (to)
				m_freem(to);
			if (control)
				m_freem(control);
			return retval;
		}

		control->m_len = msg->msg_controllen;

	} else
		control = NULL;


	len = auio.uio_resid;

	retval = sosend((struct socket *)fp->f_vnode, to, &auio, NULL, control, flags);
	if (auio.uio_resid != len && (retval == -ERESTART ||
			retval == -EINTR || retval == -EWOULDBLOCK	))
		retval = 0;
	if (retval == -EPIPE) {
		if (to)
			m_freem(to);
		if (control)
			m_freem(control);
		return retval;
	}

	if (retval == 0)
		retval = len - auio.uio_resid;

	return retval;
}

int
krecvit(int fd, struct msghdr *msg, int *fromlen)
{
	kprocess_t *proc;
	file_t *fp;
	struct uio auio;
	struct iovec *iov;
	int i;
	int len, retval;
	int retsize;
	struct mbuf *from = NULL, *control = NULL;

	proc = current->host;
	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;
	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	auio.uio_iov = msg->msg_iov;
	auio.uio_iovcnt = msg->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = 0;
	auio.uio_resid = 0;

	iov = msg->msg_iov;

	for (i=0; i<msg->msg_iovlen; i++, iov++) {
		if (auio.uio_resid + iov->iov_len < auio.uio_resid)
			return -EINVAL;
		auio.uio_resid += iov->iov_len;
	}

	len = auio.uio_resid;
	retval = soreceive((struct socket *)fp->f_vnode, &from, &auio, NULL,
			msg->msg_control ? &control : NULL, &msg->msg_flags);

	if (retval < 0) {
		if (auio.uio_resid != len && (retval == -ERESTART ||
				retval == -EINTR || retval == -EWOULDBLOCK))
			retval = 0;
	}

	if (retval)
		goto out;
	retsize = len - auio.uio_resid;
	if (msg->msg_name) {
		len = msg->msg_namelen;
		if (len <= 0 || from == NULL)
			len = 0;
		else {
			if (len > from->m_len)
				len = from->m_len;

			retval = copy_to_user((caddr_t)msg->msg_name, (caddr_t)mtod(from, caddr_t), len);
			if (retval < 0)
				goto out;
		}

		msg->msg_namelen = len;

		if (fromlen) {
			retval = copy_to_user((caddr_t)fromlen, (caddr_t)&len, sizeof(int));
			if (retval < 0)
				goto out;
		}
	}


	if (msg->msg_control) {
		len = msg->msg_controllen;
		if (len <= 0 || control == NULL)
			len = 0;
		else {
			if (len >= control->m_len)
				len = control->m_len;
			else
				msg->msg_flags |= MSG_CTRUNC;

			retval = copy_to_user((caddr_t)msg->msg_control, mtod(control, caddr_t), len);
			if (retval < 0)
				goto out;
		}

		msg->msg_controllen = len;
	}

out:
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);

	if (retval < 0)
		return retval;
	return retsize;
}


int
ksocketpair(int domain, int type, int protocol, int *retval)
{
	file_t  *fp1, *fp2;
	struct socket *so1, *so2;
	int fd1, fd2, error, sv[2];
	kprocess_t *proc = current->host;
	if ((error = socreate(domain, &so1, type, protocol)) != 0)
		return (error);
	if ((error = socreate(domain, &so2, type,  protocol)) != 0)
		goto free1;
	if ((error = falloc(proc, &fp1, &fd1)) != 0)
		goto free2;
	sv[0] = fd1;
	fp1->f_flags = FREAD|FWRITE;
	fp1->f_type = (S_IFSOCK | S_IRWXU);
	fp1->f_ops = &sockops;
	fp1->f_vnode = (struct vnode *)so1;
	if ((error = falloc(proc, &fp2, &fd2)) != 0)
		goto free3;
	fp2->f_flags = FREAD|FWRITE;
	fp2->f_type = (S_IFSOCK | S_IRWXU);
	fp2->f_ops = &sockops;
	fp2->f_vnode = (struct vnode *)so2;
	sv[1] = fd2;
	if ((error = soconnect2(so1, so2)) < 0)
		goto free4;
	if (type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 if ((error = soconnect2(so2, so1)) < 0)
			goto free4;
	}
//	error = copyout((caddr_t)sv, (caddr_t)SCARG(uap, rsv),
//	    2 * sizeof (int));
	retval[0] = sv[0];		/* XXX ??? */
	retval[1] = sv[1];		/* XXX ??? */
	return (error);
free4:
	ffree(proc, fp2, fd2);
free3:
	ffree(proc, fp1, fd1);
free2:
	(void)soclose(so2);
free1:
	(void)soclose(so1);
	return (error);
}

int
kgetsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
	int ret;
	struct mbuf *mp = NULL;
	kprocess_t *proc = current->host;
	file_t *fp;
	if (sockfd < 0 || sockfd > 1024)
		return -EINVAL;
	if (!proc)
		return -ENOSYS;
	fp = (file_t *)proc->file[sockfd];
	if (!fp)
		return -EBADF;

	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;
	ret = sogetopt((struct socket *)fp->f_vnode, level, optname, &mp);
	if (ret < 0)
		return ret;
	if (optlen) {
		if (*optlen > mp->m_len)
			*optlen = mp->m_len;
	}
	if (optval) {
		memcpy((char *)optval, mtod(mp, char *), *optlen);
	}
	if (mp)
		m_freem(mp);
	return 0;
}

int
ksetsockopt(int sockfd, int level, int optname, void *optval, socklen_t optlen)
{
	int ret;
	struct mbuf *mp = NULL;
	kprocess_t *proc = current->host;
	file_t *fp;
	if (sockfd < 0 || sockfd > 1024)
		return -EINVAL;
	if (!proc)
		return -ENOSYS;
	fp = (file_t *)proc->file[sockfd];
	if (!fp)
		return -EBADF;

	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;

	if (optval){
		mp = m_get(M_WAIT, MT_SOOPTS);
		if (!mp)
			return -ENOBUFS;
		memcpy(mtod(mp, caddr_t), (char *)optval, optlen);
	}

	ret = sosetopt((struct socket *)fp->f_vnode, level, optname, mp);
	if (ret < 0)
		return ret;

	if (mp)
		m_freem(mp);
	return 0;
}

int
kshutdown(int fd, int how)
{
	kprocess_t *proc;
	file_t *fp;

	proc = current->host;
	fp = (file_t *)proc->file[fd];
	if (fp == NULL)
		return -EBADF;

	if (!S_ISSOCK(fp->f_type))
		return -ENOTSOCK;
	return soshutdown((struct socket *)fp->f_vnode, how);
}

/*
 * this function is called by vfs level mostly.
 * NOTE:buf address is in kernel space.
 */
int sooread(file_t *fp, void *buf, u32 nbytes)
{
	/*
	 * in order to be compatible with BSD network
	 * functions, we need to build a uio struct
	 * and pass it to low level protocol function.
	 */

	struct uio auio;
	struct iovec aiov;

	aiov.iov_base = (caddr_t)buf;
	aiov.iov_len = nbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_offset = 0;

	return soreceive((struct socket *)fp->f_vnode, NULL, &auio, NULL, NULL, NULL);

}

int soowrite(file_t *fp, void *buf, u32 nbytes)
{
	struct uio auio;
	struct iovec aiov;

	aiov.iov_base = (caddr_t)buf;
	aiov.iov_len = nbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_offset = 0;

	return sosend((struct socket *)fp->f_vnode, NULL, &auio, NULL, NULL, 0);
}

int sooioctl(file_t *fp, u32 cmd, caddr_t data)
{
	struct socket *so = (struct socket *)fp->f_vnode;

	LOG_INFO("in, cmd:%d", cmd);
	switch (cmd) {
	case FIONBIO:
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		return 0;

	case FIOASYNC:
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~ SB_ASYNC;
			so->so_snd.sb_flags &= ~ SB_ASYNC;
		}
		return 0;

	case FIONREAD:
		*(int *)data = so->so_rcv.sb_cc;
		return 0;
	case SIOCSPGRP:
		//TODO:so process group
		return 0;
	case SIOCGPGRP:
		//
		return 0;
	case SIOCATMARK:
		*(int *)data = (so->so_state & SS_RCVATMARK) != 0;
		return 0;
	}

	if (IOCGROUP(cmd) == 'i')
		return ifioctl(so, cmd, data);
	if (IOCGROUP(cmd) == 'r') {
		//TODO:support route ioctl
		//return rtioctl(cmd, data);
	}

	return (*so->so_proto->pr_usrreq)(so, PRU_CONTROL, (struct mbuf *)cmd, (struct mbuf *)data, NULL);

}


int sooclose(file_t *fp)
{
	int retval = 0;

	if (fp->f_vnode)
		retval = soclose((struct socket *)fp->f_vnode);
	fp->f_vnode = NULL;



	return retval;
}



int soofstat(file_t *fp, struct stat *buf)
{
	struct socket *so = (struct socket *)fp->f_vnode;

	memset((void *)buf, 0, sizeof(struct stat));

	buf->st_mode |= S_IFSOCK;

	return (*so->so_proto->pr_usrreq)(so, PRU_SENSE, (struct mbuf *)buf, NULL, NULL);

}


struct file_ops sockops = {
		.read = sooread,
		.write = soowrite,
		.fstat = soofstat,
		.seek = NULL,
		.close = sooclose,
		.ioctl = sooioctl,
};

















