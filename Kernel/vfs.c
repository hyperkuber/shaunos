/*
 * vfs.c
 *
 * Copyright (c) 2012 Shaun Yuan
 * 
 */

#include <kernel/vfs.h>
#include <string.h>
#include <kernel/assert.h>
#include <kernel/malloc.h>
#include <kernel/io_dev.h>
#include <kernel/ext2.h>
#include <kernel/kernel.h>
#include <kernel/kthread.h>
#include <kernel/stat.h>
#include <kernel/sys/sockio.h>
#include <kernel/uio.h>
#include <kernel/time.h>
#include <kernel/limits.h>

//vnode_t *rootv;

extern struct timespec xtime;
#define CURRENT_TIME_SEC (xtime.ts_sec)

static struct linked_list fs_list_head = {
		&fs_list_head,
		&fs_list_head,
};

static struct linked_list mount_point_list_head = {
		&mount_point_list_head,
		&mount_point_list_head,
};


static fs_t *lookup_filesystem(const char *fstype)
{
	fs_t *fs = NULL;
	struct linked_list *pos = fs_list_head.next;
	assert(pos != NULL);
	for (; pos != &fs_list_head; pos = pos->next){
		fs = container_of(pos, fs_t, fs_list);
		assert(fs != NULL);
		if (strcmp(fs->name, fstype) == 0)
			return fs;
	}

	return NULL;
}

static mount_t *lookup_mount_point(const char *prefix)
{
	mount_t *mt = NULL;
	struct linked_list *pos = mount_point_list_head.next;
	if (pos == NULL)
		return NULL;

	for (; pos != &mount_point_list_head;
			pos = pos->next){
		mt = container_of(pos, mount_t, list);
		//LOG_INFO("mount point:%s", mt->path_prefix);
		if (strcmp(mt->path_prefix, prefix) == 0)
			return mt;
	}

	return NULL;
}

int register_filesystem(const char *fsname, fs_ops_t *fs_ops)
{
	fs_t *fs = NULL;
	assert(fsname != NULL);
	assert(fs_ops != NULL);
	assert(fs_ops->mount != NULL);

	fs = kzalloc(sizeof(fs_t));
	if (fs == NULL)
		return -ENOBUFS;

	fs->ops = fs_ops;
	strncpy(fs->name, fsname, 15);

	list_add_tail(&fs_list_head, &fs->fs_list);
	LOG_INFO("register filesystem:%s end", fsname);
	return 0;
}

int vfs_format(const char *devname, const char *fstype)
{
	fs_t *fs = NULL;
	dev_t *dev = NULL;
	int retval;

	fs = lookup_filesystem(fstype);
	if (fs == NULL)
		return -1;
	if (fs->ops->format == NULL)
		return -1;

	if ((retval = open_device(devname, &dev)) < 0)
		return retval;

	retval = fs->ops->format(dev);

	close_device(dev);

	return retval;
}

vnode_t *
vfs_alloc_vnode(void)
{
	//this should be in cache
	vnode_t *v = kmalloc(sizeof(vnode_t));
	assert(v != NULL);

	memset((void *)v, 0, sizeof(vnode_t));

	return v;
}

int
vfs_free_vnode(vnode_t *v)
{
	if (v == NULL){
		LOG_ERROR("invalid param");
		return -1;
	}

	kfree(v);
	return 0;
}


vnode_t *
vfs_get_rootv()
{
	mount_t *mp = NULL;
	mp = lookup_mount_point("/");
	if (mp == NULL)
		return NULL;
	return mp->vnode;
}

char *
vfs_get_name(const char *path)
{
	int path_len = strnlen_user(path, PATH_MAX) + 1;

	if (path_len > PATH_MAX)
		return NULL;

	char *name = (char *)kmalloc(path_len);
	if (!name)
		return NULL;

	if (copy_from_user(name ,(char *)path, path_len) < 0){
		kfree(name);
		return NULL;
	}
	return name;
}

int vfs_put_name(char *name)
{
	if (name)
		kfree(name);
	return 0;
}


int vfs_mount(const char *devname, const char *prefix, const char *fstype, vnode_t **root)
{
	fs_t *fs = NULL;
	dev_t *dev = NULL;
	mount_t *mount = NULL;
	int retval;
	vnode_t *v;

	while(*prefix == '/')
		prefix++;

	if (strlen(prefix) > MAX_PREFIX_LEN)
		return -1;

	fs = lookup_filesystem(fstype);
	if (fs == NULL)
		return -ENODEV;

	assert(fs->ops->mount != NULL);

	retval = open_device(devname, &dev);
	if (retval < 0)
		return -ENODEV;

	mount = (mount_t *)kmalloc(sizeof(mount_t));
	assert(mount != NULL);
	memset(mount, 0, sizeof(mount_t));
	mount->dev = dev;
	strcpy(mount->path_prefix, --prefix);
	v = vfs_alloc_vnode();
	assert(v != NULL);
	v->v_mp = mount;
	mount->mp_flags |= (FWRITE | FREAD);
	v->v_device = dev->devid;
	retval = fs->ops->mount(v);
	if (retval != 0){
		kfree(mount);
		kfree(v);
		v = NULL;
		return -1;
	}

	*root = v;
	v->v_mp->vnode = v;

	list_add_tail(&mount_point_list_head, &mount->list);

	close_device(dev);
	LOG_INFO("mount fs %s successfully", fstype);
	return retval;

}

vnode_t *vfs_path_lookup(vnode_t *pv, const char *path, char *suffix)
{
	vnode_t *currv, *nextv, *pntv;
	int retval;
	char *_p = strdup(path);
	char *p = _p;
	char *slash;
	if (*p == '/'){
		while (*++p == '/');
		currv = (vnode_t *)pv->v_mp->vnode;
	} else {
		currv = pv;
	}
	pntv = currv;
	nextv = vfs_alloc_vnode();
	if(nextv == NULL)
		return NULL;
	nextv->v_mp = pv->v_mp;

	nextv->v_device = pv->v_device;

	while (1){
		slash = strchr(p, '/');
		if (slash == NULL){
			break;
		}
		*slash = '\0';
		do {
			slash++;
		} while (*slash == '/');

		if (strcmp(p, ".") == 0){
			p = slash;
			continue;
		}

		if (strcmp(p, "..") == 0){
			p = slash;
			currv = pntv;
			continue;
		}

		if (*slash == '\0' || *slash == '\n')
			break;

		LOG_NOTE("p:%s, slash:%s, currv:%x", p, slash, (unsigned long)currv);

		retval = currv->v_mp->mp_ops->lookup(currv, p, &nextv);
		if (retval < 0){
			vfs_free_vnode(nextv);
			kfree(_p);
			return NULL;
		}

		p = slash;
		pntv = currv;
		currv = nextv;

	}

	if (nextv->v_id == 0){
		vfs_free_vnode(nextv);
	}

	strncpy(suffix, p, NAME_MAX);
	//LOG_INFO("currentv id:%d", currv->id);
	kfree(_p);
	return currv;
}

static inline void
fget(file_t *fp)
{
	if (!fp)
		fp->f_refcnt++;
}
static inline void
fput(file_t *fp)
{
	if (!fp)
		--fp->f_refcnt;
}


int vfs_open(vnode_t *pv, const char *path, int flag, file_t **file)
{
	int retval;
	char *name = (char *)kzalloc(NAME_MAX);
	if (!name)
		return -ENOMEM;
	vnode_t *vnode = vfs_path_lookup(pv, path, name);
	if (vnode == NULL){
		kfree(name);
		return -ENOENT;
	}

	retval = pv->v_mp->mp_ops->open(vnode, name, flag, file);
	if (retval < 0){
		LOG_ERROR("open file failed");
		goto END;
	}
	LOG_NOTE("file:%s, mode:%d", name, (*file)->f_vnode->v_mode);
	retval = -EPERM;

	switch (flag & O_ACCMODE) {
	case O_RDWR:
		if (((*file)->f_type & (S_IWUSR | S_IRUSR)) != (S_IWUSR | S_IRUSR)){
			LOG_ERROR("open file failed, permission error");
			goto END;
		}
		(*file)->f_flags |= (FREAD | FWRITE);
		break;
	case O_WRONLY:
		if (!((*file)->f_type & S_IWUSR)){
			LOG_ERROR("open file failed, permission error");
			goto END;
		}
		(*file)->f_flags |= FWRITE;
		break;
	case O_RDONLY:
		if (!((*file)->f_type & S_IRUSR)){
			LOG_ERROR("open file failed, permission error");
			goto END;
		}
		(*file)->f_flags |= FREAD;
		break;
	}
	flag &= ~O_ACCMODE;

	(*file)->f_flags |= flag;
	retval = 0;
	fget(*file);
	kfree(name);
	return retval;
END:
	kfree(name);
	vfs_close(*file);
	return retval;

}

int vfs_open_dev(char *path, int flag, file_t **file)
{
	mount_t *mp;
	mp = lookup_mount_point(path);
	if (mp == NULL) {
		LOG_ERROR("can not find mount point, path:%s", path);
		return -ENODEV;
	}

	int retval = mp->mp_ops->open((vnode_t *)mp->fsdata, path, flag, file);
	if (retval < 0){
		LOG_ERROR("mount point open failed");
		return retval;
	}

	retval = mp->dev->ops->open(mp->dev);
	if (retval < 0){
		LOG_ERROR("open device failed");
		return retval;
	}

	return 0;
}

int vfs_create(vnode_t *pv, const char *path, int mode, file_t **file)
{
	int retval;
	char *name;

	if (!pv)
		return -EINVAL;
	//file permission bit on disk disallow us to write
	if ((pv->v_mode & (S_IWUSR)) == 0)
		return -EACCES;

	name = kmalloc(NAME_MAX);
	assert(name != NULL);

	vnode_t *vnode = vfs_path_lookup(pv, path, name);
	if (vnode == NULL){
		LOG_ERROR("invalid path, path:%s", path);
		kfree(name);
		return -ENOENT;
	}

	retval = pv->v_mp->mp_ops->create(vnode, name, mode, file);
	if (retval < 0){
		LOG_ERROR("create file failed");
	}
	//free parent vnode

	if (vnode != vfs_get_rootv())
		vfs_free_vnode(vnode);
	kfree(name);
	return retval;

}

int vfs_close(file_t *file)
{
	int retval = 0;
	if (--file->f_refcnt == 0){
		retval = file->f_ops->close(file);
	}
	return retval;
}


file_t *vfs_alloc_file()
{
	file_t *file = kmalloc(sizeof(file_t));
	if (file == NULL)
		return NULL;
	memset((void *)file, 0, sizeof(file_t));
	return file;

}
void vfs_free_file(file_t *fp)
{
	if (fp)
		kfree(fp);
}

int vfs_fstat(file_t *file, state_t *stat)
{
	if (file->f_ops->fstat != NULL){
		return file->f_ops->fstat(file, stat);
	}
	return -ENOSYS;
}
int falloc(kprocess_t *proc, file_t **pfp, int *pfd)
{
	int ret;
	file_t *fp = vfs_alloc_file();
	if (fp == NULL)
		return -ENOBUFS;
	int fd = get_unused_id(&proc->p_fd_obj);
	if (fd < 0)
		goto end;

	if (fd > proc->lmt[RLIMIT_NOFILE].rlim_max){
		ret = -ENFILE;
		goto end;
	}

	if (proc->open_files > 1024) {
		ret = -EMFILE;
		goto end1;
	}

	ret = fd_attach(fd, proc, fp);
	if (ret < 0)
		goto end1;

	proc->open_files++;

	*pfp = fp;
	*pfd = fd;
	return 0;
end1:
	put_id(&proc->p_fd_obj, fd);
end:
	vfs_free_file(fp);
	return ret;
}
int ffree(kprocess_t *proc, file_t *fp, int fd)
{
	int ret = fd_detach(fd, proc);
	if (ret < 0)
		return ret;

	vfs_free_file(fp);
	proc->open_files--;
	put_id(&proc->p_fd_obj, fd);
	return 0;
}

int vfs_read(file_t *file, void *buf, unsigned long len)
{
	if (!file || !buf)
		return -EINVAL;

	if (len == 0)
		return 0;

	/*
	 * permission on open
	 */
	if ((file->f_flags & FREAD) == 0)
		return -EACCES;

	/*
	 * permission on file
	 */
	if (!(file->f_type & S_IRUSR))
		return -EPERM;

	if (file->f_curr_pos > file->f_vnode->v_size){
		memset((void *)buf, 0, len);
		return len;
	}

	if (file->f_ops->read != NULL)
		return file->f_ops->read(file, buf, len);
	return -ENOSYS;
}

int vfs_write(file_t *file, void *buf, unsigned long len)
{

	if (!file || !buf)
		return -EINVAL;

	if (len == 0)
		return 0;

	/*
	 * open for read only?
	 */
	if ((file->f_flags & FWRITE) == 0)
		return -EACCES;
	/*
	 * user has no write permission?
	 */
	if (!(file->f_type & S_IWUSR))
		return -EPERM;

	if (file->f_ops->write != NULL)
		return file->f_ops->write(file, buf, len);
	return -ENOSYS;
}

int vfs_seek(file_t *file, off_t pos, int whence)
{
	if (!S_ISREG(file->f_type))
		return -ESPIPE;

//	if (!(file->f_vnode->v_mode & EXT2_FT_REG))
//		return -ESPIPE;

	if (file->f_ops->seek != NULL)
		return file->f_ops->seek(file, pos, whence);

	return -ENOSYS;
}


int
kread(int fd, void *buf, int len)
{
	kprocess_t *proc;
	file_t *file;
	char *data = NULL;
	int nr_page = 0;
	int retval;
	int order = 0;
	proc = current->host;
	if (proc) {
		file = (file_t *)proc->file[fd];
		if (!file){
			LOG_ERROR("invalid fd:%d", fd);
			return -EBADF;
		}

		if (!(file->f_flags & FREAD))
			return -EACCES;

		if (len > SSIZE_MAX)
			return -EINVAL;

		if (len > PAGE_SIZE){
			nr_page = PAGE_ALIGN(len) >> PAGE_SHIFT;
			while ((1<<order) < nr_page)
				order++;

			data = (char *)alloc_pages(ALLOC_NORMAL, order);
		} else {
			data = (char *)kmalloc(len);
			memset(data, 0, len);
		}

		if (data == NULL)
			return -ENOMEM;

		retval = vfs_read(file, data, len);
		if (retval < 0){
			goto END;
		}
		retval = copy_to_user(buf, data, retval);
	} else {
		return -ENOSYS;
	}

END:
	if (len > PAGE_SIZE)
		free_pages(order, (unsigned long)data);
	else
		kfree(data);
	return retval;
}

ssize_t kreadv(int fd, const struct iovec *uiov, int iovcnt)
{
	int ret;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int nr_seg;
	char *base;
	ssize_t len;
	ssize_t tot_len;
	if (iovcnt > UIO_MAXIOV || iovcnt <= 0)
		return -EINVAL;

	if (iovcnt <= UIO_SMALLIOV){
		iov = aiov;
	} else {
		iov = kmalloc(sizeof(struct iovec) * iovcnt);
		if (iov == NULL)
			return -ENOMEM;
	}

	ret = copy_from_user((char *)iov, (char *)uiov, iovcnt * sizeof(struct iovec));
	if (ret < 0){
		ret = -EFAULT;
		goto end;
	}
	tot_len = 0;
	nr_seg = 0;
	ret = -EINVAL;

	while (nr_seg++ < iovcnt){
		base = (char *)iov[nr_seg].iov_base;
		len = (ssize_t)iov[nr_seg].iov_len;
		if (len < 0)
			goto end;
		if (probe_for_write((u32)base, len) < 0)
			goto end;

		tot_len += len;
		if (tot_len < 0)
			goto end;
	}

	if (tot_len == 0){
		ret = 0;
		goto end;
	}

	nr_seg = 0;
	tot_len = 0;
	while (nr_seg++ < iovcnt){
		base = (char *)iov[nr_seg].iov_base;
		len = (ssize_t)iov[nr_seg].iov_len;

		ret = kread(fd, base, len);
		if (ret < 0)
			goto end;
		tot_len += ret;
	}
	ret = tot_len;
end:
	if (iov != aiov)
		kfree(iov);
	return ret;
}


int kwrite(int fd, void *buf, int len)
{
	kprocess_t *proc;
	file_t *file;
	char *data = NULL;
	int retval, nr_page;
	int order = 0;
	proc = current->host;
	if (proc){
		file = (file_t *)proc->file[fd];
		if (!file){
			LOG_ERROR("invalid fd:%d", fd);
			return -EBADF;
		}
		if ((file->f_flags & FWRITE) == 0)
			return -EACCES;


		if (len > PAGE_SIZE){
			nr_page = PAGE_ALIGN(len) >> PAGE_SHIFT;
			while ((1<<order) < nr_page)
				order++;

			data = (char *)alloc_pages(ALLOC_NORMAL, order);
		} else {
			data = (char *)kmalloc(len);
		}

		if (data == NULL)
			return -ENOMEM;

		retval = copy_from_user(data, (char *)buf, len);
		if (retval < 0){
			LOG_ERROR("copy data from user space failed");
			goto END;
		}

		retval = vfs_write(file, data, len);
	} else {
		return -ENOSYS;
	}

END:
	if (len > PAGE_SIZE)
		free_pages(order, (unsigned long)data);
	else
		kfree(data);
	return retval;
}

/*
 * Based on the details here,
 * http://pubs.opengroup.org/onlinepubs/9699919799/
 */

ssize_t kwritev(int fd, const struct iovec *uiov, int iovcnt)
{
	int ret;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int nr_seg;
	char *base;
	ssize_t len;
	ssize_t tot_len;
	if (iovcnt > UIO_MAXIOV || iovcnt <= 0)
		return -EINVAL;

	if (iovcnt <= UIO_SMALLIOV){
		iov = aiov;
	} else {
		iov = kmalloc(sizeof(struct iovec) * iovcnt);
		if (iov == NULL)
			return -ENOMEM;
	}

	ret = copy_from_user((char *)iov, (char *)uiov, iovcnt * sizeof(struct iovec));
	if (ret < 0){
		ret = -EFAULT;
		goto end;
	}
	tot_len = 0;
	nr_seg = 0;
	ret = -EINVAL;

	while (nr_seg++ < iovcnt){
		base = (char *)iov[nr_seg].iov_base;
		len = (ssize_t)iov[nr_seg].iov_len;
		if (len < 0)
			goto end;
		if (probe_for_read((u32)base, len) < 0)
			goto end;

		tot_len += len;
		if (tot_len < 0)
			goto end;
	}
	if (tot_len == 0){
		ret = 0;
		goto end;
	}

	nr_seg = 0;
	tot_len = 0;
	while (nr_seg++ < iovcnt){
		base = (char *)iov[nr_seg].iov_base;
		len = (ssize_t)iov[nr_seg].iov_len;

		ret = kwrite(fd, base, len);
		if (ret < 0)
			goto end;
		tot_len += ret;
	}
	ret = tot_len;
end:
	if (iov != aiov)
		kfree(iov);
	return ret;

}


int kopen(const char *path, int flag, int mode)
{
	file_t *file;
	char *p;
	int retval, fd;
	kprocess_t *proc = current->host;;

	if (!path)
		return -EINVAL;

	if (proc->open_files > 1024)
		return -EMFILE;

	if (proc->open_files > proc->lmt[RLIMIT_NOFILE].rlim_max)
		return -ENFILE;


	char *name = (char *)path;
	p = name;
	if (*p == '/'){
		while (*p == '/')
			p++;

		if (*p++ == 'd' && *p++ == 'e' && *p == 'v'){
			retval = vfs_open_dev(name, flag, &file);
			if (retval < 0){
				LOG_ERROR("open device failed, error:%d", retval);
				return retval;
			}
			goto END;
		}
		vnode_t *rootv = vfs_get_rootv();
		if (rootv == NULL)
			return -ENODEV;

		if (flag & O_CREAT){
			retval = vfs_create(rootv, name, mode, &file);
			if (retval < 0){
				LOG_ERROR("create file failed, error:%d", retval);
				return retval;
			}
			flag &= ~O_CREAT;
			file->f_flags = flag;
			goto END;
		}

		retval = vfs_open(rootv, name, flag, &file);
		if (retval < 0){
			LOG_ERROR("open file failed, error:%d", retval);
			return retval;
		}

		goto END;
	}

	retval = vfs_open(proc->p_curr_cwd->f_vnode, name, flag, &file);
	if (retval < 0){
		LOG_ERROR("open failed failed, error:%d", retval);
		return retval;
	}
END:
	fd = get_unused_id(&proc->p_fd_obj);
	if (fd >= 1024){
		vfs_close(file);
		return -EMFILE;
	}
	proc->open_files++;

	return fd_attach(fd, proc, file);
}

int kfstat(int fd, struct stat *st)
{
	kprocess_t *proc;
	proc = current->host;
	int retval;
	if (proc){
		file_t *file = (file_t *)proc->file[fd];
		if (!file)
			return -EBADF;

		retval = vfs_fstat(file, st);
		if(retval < 0){
			return retval;
		}
		return 0;
	}

	return -ENOSYS;
}

int kstat(const char *path, struct stat *buf)
{
	int retval;
	vnode_t *rootv;
	vnode_t *pv;
	rootv = vfs_get_rootv();
	if (!rootv)
		return -ENODEV;

	if (!path || !buf)
		return -EINVAL;

	char *suffix = kzalloc(NAME_MAX);
	if (!suffix)
		return -ENOMEM;
	char *name = (char *)path;
	pv = vfs_path_lookup(rootv, name, suffix);
	if (pv == NULL){
		retval = -ENOENT;
		goto END1;
	}
	LOG_INFO("path:%s suffix:%s", name, suffix);

	if (pv->v_mp->mp_ops->stat){
		retval = pv->v_mp->mp_ops->stat(pv, suffix, buf);
		if (retval < 0){
			goto END3;
		}
		retval = 0;
	} else
		retval = -ENOSYS;

END3:
	if (pv != rootv)
		vfs_free_vnode(pv);
END1:
	kfree(suffix);
	LOG_INFO("end, retval:%d.", retval);
	return retval;
}

off_t klseek(int fd, off_t offset, int whence)
{
	kprocess_t *proc = current->host;
	if (proc){
		file_t *file = (file_t *)proc->file[fd];
		if (!file)
			return -EBADF;
		return vfs_seek(file, offset, whence);
	}

	return -ENOSYS;
}

int kunlink(char *path)
{
	int retval;
	char *name = path;

	vnode_t *rootv = vfs_get_rootv();
	if (!rootv){
		retval = -ENODEV;
		goto END1;
	}

	char *suffix = (char *)kmalloc(NAME_MAX);
	if (!suffix){
		retval = -ENOMEM;
		goto END1;
	}

	vnode_t *pv = vfs_path_lookup(rootv, name, suffix);
	if (!pv){
		retval = -ENOENT;
		goto END2;
	}

	retval = pv->v_mp->mp_ops->delete(pv, suffix);
	if (retval < 0){
		goto END3;
	}
	retval = 0;

END3:
	vfs_free_vnode(pv);
END2:
	kfree(suffix);
END1:
	vfs_put_name(name);
	return retval;
}

int kmkdir(char *path, int mode)
{
	file_t *fp;
	int ret;
	vnode_t *rootv = vfs_get_rootv();
	ret = vfs_create(rootv, path, mode | S_IFDIR, &fp);
	if (ret < 0)
		goto end;

	ret = vfs_close(fp);
end:
	return ret;
}

int kclose(int fd)
{
	kprocess_t *proc;
	proc = current->host;
	if (proc){
		file_t *file = (file_t *)proc->file[fd];
		if (!file)
			return -EBADF;
		vfs_close(file);
		proc->open_files--;

		if (fd_detach(fd, proc) < 0){
			return -EBADF;
		}
		put_id(&proc->p_fd_obj, fd);
	}
	return 0;
}


int kioctl(int fd, int cmd, caddr_t data)
{
	kprocess_t * proc;
	proc = current->host;
	file_t *fp;
	int tmp, retval;
	if (proc) {
		fp = (file_t *)((u32 *)proc->file)[fd];
		if (fp == NULL)
			return -EBADF;

		if ((fp->f_flags & (FREAD|FWRITE)) == 0)
			return -EBADF;

		switch (cmd) {
		case FIONBIO:
			if ((tmp = *(int *)data) != 0)
				fp->f_flags |= FNONBLOCK;
			else
				fp->f_flags &= ~FNONBLOCK;
			retval = -ENOSYS;
			if (fp->f_ops->ioctl)
				retval = (*fp->f_ops->ioctl)(fp, FIONBIO, (caddr_t)&tmp);
			break;
		case FIOASYNC:
			if ((tmp = *(int *)data) != 0)
				fp->f_flags |= FASYNC;
			else
				fp->f_flags &= ~FASYNC;
			retval = -ENOSYS;
			if (fp->f_ops->ioctl)
				retval = (*fp->f_ops->ioctl)(fp, FIOASYNC, (caddr_t)&tmp);
			break;
		case FIOSETOWN:
		case FIOGETOWN:
			retval = -ENOSYS;
			break;
		default:
			retval = -ENOSYS;
			if (fp->f_ops->ioctl)
				retval = (*fp->f_ops->ioctl)(fp, cmd, data);

		}

		return retval;

	}
	//for  kernel thread which doesnt belong to any proc
	return -EOPNOTSUPP;
}

int kfcntl(int fd, int cmd, int arg)
{
	kprocess_t * proc;
	proc = current->host;
	file_t *fp;
	int retval;
	if (proc) {
		LOG_INFO("in");
		fp = (file_t *)(proc->file[fd]);
		if (fp == NULL)
			return -EBADF;

		if ((fp->f_flags & (FREAD|FWRITE)) == 0)
			return -EACCES;

		switch (cmd) {
		case F_GETFD:
			retval = fp->f_flags & FD_CLOEXEC;
			break;
		case F_SETFD:
			//fp->f_flags = arg;
			retval = 0;
			break;
		case F_GETFL:
			retval = (fp->f_flags & ~O_ACCMODE);
			break;
		case F_SETFL:
			//O_RDONLY, O_WRONLY and O_RDWR is ignored.
			fp->f_flags |= (arg & ~O_ACCMODE);
			retval = 0;
			break;
		case F_DUPFD:
			/*
			 * TODO:find the lowest number available greater than or equal to arg
			 */
			retval = get_unused_id(&proc->p_fd_obj);
			if (retval < 0 || retval> proc->lmt[RLIMIT_NOFILE].rlim_max)
				retval = -EMFILE;
			else {
				fd_attach(retval, proc, fp);
				fp->f_refcnt++;
			}
			break;
		default:
			retval = -ENOSYS;
		}
		LOG_INFO("end, ret:%d", retval);
		return retval;

	}
	//for  kernel thread which doesnt belong to any proc
	return -EOPNOTSUPP;
}

int kdup2(int oldfd, int newfd)
{
	kprocess_t *proc = current->host;
	file_t *nfp, *ofp;
	int ret;
	LOG_INFO("in");
	if (proc) {

		if (oldfd >= 1024 || newfd >= 1024)
			return -EINVAL;

		if (newfd > proc->lmt[RLIMIT_NOFILE].rlim_max)
			return -EMFILE;
		ofp = (file_t *)proc->file[oldfd];
		nfp = (file_t *)proc->file[newfd];
		if (ofp == NULL) {
			LOG_ERROR("bad fd,%d", oldfd);
			return -EBADF;
		}

		if (ofp == nfp)
			return newfd;
		if (nfp != NULL) {
			//kclose(newfd);
			vfs_close(nfp);
			fd_detach(newfd, proc);
			proc->open_files--;
		}

		ret = fd_attach(newfd, proc, ofp);
		if (ret < 0)
			return ret;
		resv_id(&proc->p_fd_obj, newfd);
		proc->open_files++;
		return newfd;
	}
	return -EOPNOTSUPP;
}

int kgetdents(int fd, u8 *buf, int size)
{
	kprocess_t *proc;
	proc = current->host;
	file_t *fp;

	if (proc) {
		fp = (file_t *)((u32 *)proc->file)[fd];
		if (fp == NULL)
			return -EBADF;

		if (!S_ISDIR(fp->f_type))
			return -ENOTDIR;

		return fp->f_ops->read(fp, buf, min(size, fp->f_vnode->v_size));

	}
	//for  kernel thread which doesnt belong to any proc

	return -EOPNOTSUPP;
}


int kgetdirentries(int fd, char *buf, int nbytes, long basep)
{
	int ret;
	kprocess_t *proc = current->host;
	file_t *fp;

	if (proc) {
		fp = (file_t *)((u32 *)proc->file[fd]);
		if (fp == NULL)
			return -EBADF;

		if (!S_ISDIR(fp->f_type))
			return -ENOTDIR;

		fp->f_curr_pos = basep;


		ret = fp->f_ops->read(fp, buf, min(nbytes, fp->f_vnode->v_size));
		if (ret < 0)
			return ret;

		basep = fp->f_curr_pos;
		return ret;
	}
	return -EOPNOTSUPP;

}

int kchdir(char *path)
{
	int ret;
	file_t *fp;
	//file_t *oldfp;
	kprocess_t *proc = current->host;
//	fd = kopen(path, O_RDWR, 0);
//	if (fd < 0)
//		return fd;

	ret = vfs_open(proc->p_currdir, path, O_RDWR, &fp);
	if (ret < 0)
		return ret;

	if (!S_ISDIR(fp->f_type)) {
		vfs_close(fp);
		return -ENOTDIR;
	}

	if (proc->p_old_cwd)
		vfs_close(proc->p_old_cwd);
	proc->p_old_cwd = proc->p_curr_cwd;
	proc->p_curr_cwd = fp;
	//oldfp = proc->p_currdir;
	//proc->p_currdir = fp->f_vnode;
	//fget(fp);
	//fput(oldfp);

	//kclose(fd);
	return 0;
}

int kfchdir(int fd)
{
	kprocess_t *proc = current->host;
	file_t *fp;
	if (proc) {
		if (fd < 0 || fd > 1024)
			return -EBADF;

		fp = (file_t *)proc->file[fd];
		if (fp == NULL)
			return -EBADF;
		if (!(S_ISDIR(fp->f_type)))
			return -ENOTDIR;

		proc->p_currdir = fp->f_vnode;
		proc->p_curr_cwd = fp;
		return 0;
	}
	return -EOPNOTSUPP;
}

int vfs_statfs(struct vnode *pv, struct statfs *sf)
{
	int ret;
	//if fp refers to a tty device, EINVAL(in linux) should be returned?
	if (!pv->v_mp->mp_ops->statfs){
		ret = -ENOSYS;
		goto end;
	}

	ret = pv->v_mp->mp_ops->statfs(pv, sf);

end:
	return ret;
}

int kstatfs(char *path, struct statfs *sf)
{
	int ret;
	kprocess_t *proc = current->host;
	file_t *fp;
	int fd = kopen(path, O_RDONLY, 0);
	if (fd < 0)
		return fd;


	if (proc) {
		fp = (file_t *)proc->file[fd];
		if (fp == NULL) { //this could not be happened
			ret = -EBADF;
			goto end;
		}

		ret = vfs_statfs(fp->f_vnode, sf);
		if (ret < 0) {
			goto end;
		}
	}
	ret = 0;

end:
	kclose(fd);
	return ret;
}

int kfstatfs(int fd, struct statfs *sf)
{

	kprocess_t *proc = current->host;
	file_t *fp;

	if (fd < 0 || fd > 1024 || !sf)
		return -EINVAL;
	if (!proc)
		return -ENOSYS;

	fp = (file_t *)proc->file[fd];
	if (!fp)
		return -EBADF;

	return vfs_statfs(fp->f_vnode, sf);
}

int vfs_fsync(struct vnode *pv)
{
	if ((pv->v_mp->mp_flags & FWRITE) == 0)
		return -EROFS;

	if (pv->v_mp->mp_ops->sync)
		return pv->v_mp->mp_ops->sync(pv);

	return -EINVAL;
}

int kfsync(int fd)
{
	int ret;
	kprocess_t *proc = current->host;
	file_t *fp;
	if (fd < 0 || fd > 1024)
		return -EINVAL;

	fp = (file_t *)proc->file[fd];
	if (!fp)
		return -EBADF;

	ret = vfs_fsync(fp->f_vnode);
	return ret;
}

int vfs_ftruncate(struct vnode *pv, off_t len)
{

	if ((pv->v_mode & U_WR) == 0)
		return -EPERM;

	return 0;
}

int kftruncate(int fd, off_t len)
{
	kprocess_t *proc = current->host;
	file_t *fp;

	if (!proc)
		return -ENOSYS;

	if (fd < 0 || fd > 1024)
		return -EINVAL;

	fp = (file_t *)proc->file[fd];
	if (!fp)
		return -EBADF;

	if (!(fp->f_flags & FWRITE))
		return -EPERM;

	if (S_ISDIR(fp->f_type))
		return -EISDIR;
	if (!(fp->f_type & (S_IWUSR)))
		return -EPERM;

	if(fp->f_flags & O_APPEND)
		return -EPERM;

	if (fp->f_vnode->v_uid != proc->uid)
		if (proc->uid != 0)
			return -EACCES;

	if (len > MAX_NON_LFS)
		if (!(fp->f_flags & O_LARGEFILE))
			return -EINVAL;

	return vfs_ftruncate(fp->f_vnode, len);
}


int kutimes(char *path, struct timeval *tval)
{
	int ret;
	int fd;
	file_t *fp;
	kprocess_t *proc = current->host;
	if (!path)
		return -EINVAL;

	if (!proc)
		return -ENOSYS;

	fd = kopen(path, O_RDWR, 0);
	if (fd < 0)
		return fd;

	fp = (file_t *)proc->file[fd];

	if (!fp)
		return -EBADF;
	ret = -EACCES;
	if (fp->f_vnode->v_uid != proc->uid)	//not my file
		if (proc->uid != 0)		//i am not root
			goto end;		//sorry, you can not do this

	if (!(fp->f_type & S_IWUSR)){
		ret = -EPERM;	// file is read only
		goto end;
	}

	if (tval) {
		fp->f_vnode->v_atime = tval[0].tv_sec + tval[0].tv_usec / 1000 / 1000;
		fp->f_vnode->v_mtime = tval[1].tv_sec + tval[1].tv_usec / 1000 / 1000;
	} else {
		fp->f_vnode->v_atime = CURRENT_TIME_SEC;
		fp->f_vnode->v_mtime = CURRENT_TIME_SEC;
	}

	ret = kfsync(fd);
	if (ret < 0)
		goto end;

	ret = 0;

end:
	kclose(fd);
	return ret;
}

int kchmod(char *path, mode_t mode)
{
	int ret;
	file_t *fp;
	char *p;
	kprocess_t *proc = current->host;
	if (!path)
		return -EINVAL;

	if (!proc)
		return -ENOSYS;

	p = path;

	if (*p == '/'){
		ret = vfs_open(vfs_get_rootv(), path, O_RDWR, &fp);
	} else {
		ret = vfs_open(proc->p_curr_cwd->f_vnode, path, O_RDWR, &fp);
	}
	if (ret < 0)
		return ret;

	fp->f_vnode->v_mode = (mode & ~S_IFMT);

	ret = vfs_fsync(fp->f_vnode);

	vfs_close(fp);
	return ret;
}

int kfchmod(int fd, mode_t mode)
{
	int ret;
	file_t *fp;
	kprocess_t *proc = current->host;

	if (!proc)
		return -ENOSYS;

	if (fd < 0 || fd > 1024)
		return -EINVAL;

	fp = (file_t *)proc->file[fd];

	if (!fp)
		return -EBADF;

	fp->f_type |= (mode & ~S_IFMT);

	ret = 0;
	if (!S_ISSOCK(fp->f_type)){
		fp->f_vnode->v_mode = mode;
		ret = vfs_fsync(fp->f_vnode);
	}

	return ret;
}
