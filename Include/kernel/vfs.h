/*
 * vfs.h
 *
 * Copyright (c) 2012 Shaun Yuan
 * 
 */

#ifndef SHAUN_VFS_H_
#define SHAUN_VFS_H_
#include <kernel/kernel.h>
#include <list.h>
#include <kernel/types.h>
#include <kernel/io_dev.h>
#include <kernel/stat.h>
#include <kernel/uio.h>


#define MAX_PREFIX_LEN 16
#define PATH_MAX	4096
#define NAME_MAX	256

struct mount_point;
struct mount_point_operations;
struct file;
struct file_ops;
struct file_state;
struct vnode;
struct statfs;
typedef struct stat state_t;
#define	FREAD		0x0001
#define	FWRITE		0x0002

#define	O_RDONLY	0x0000		/* open for reading only */
#define	O_WRONLY	0x0001		/* open for writing only */
#define	O_RDWR		0x0002		/* open for reading and writing */
#define	O_ACCMODE	0x0003		/* mask for above modes */

#define	O_NONBLOCK	0x0004		/* no delay */
#define	O_APPEND	0x0008		/* set append mode */

#define	O_SHLOCK	0x0010		/* open with shared file lock */
#define	O_EXLOCK	0x0020		/* open with exclusive file lock */
#define	O_ASYNC		0x0040		/* signal pgrp when data ready */
#define	O_FSYNC		0x0080		/* synchronous writes */

#define	O_CREAT		0x0200		/* create if nonexistant */
#define	O_TRUNC		0x0400		/* truncate to zero length */
#define	O_EXCL		0x0800		/* error if already exists */

#define O_SYNC		 010000
#define O_DIRECT	 040000	/* direct disk access hint */
#define O_LARGEFILE	0100000
#define O_DIRECTORY	0200000	/* must be a directory */
#define O_NOFOLLOW	0400000 /* don't follow links */
#define O_NOATIME	01000000


#define	FAPPEND		O_APPEND	/* kernel/compat */
#define	FASYNC		O_ASYNC		/* kernel/compat */
#define	FFSYNC		O_FSYNC		/* kernel */
#define	FNONBLOCK	O_NONBLOCK	/* kernel */
#define	FNDELAY		O_NONBLOCK	/* compat */
#define	O_NDELAY	O_NONBLOCK	/* compat */


#define	F_DUPFD		0		/* duplicate file descriptor */
#define	F_GETFD		1		/* get file descriptor flags */
#define	F_SETFD		2		/* set file descriptor flags */
#define	F_GETFL		3		/* get file status flags */
#define	F_SETFL		4		/* set file status flags */

#define	F_GETOWN	5		/* get SIGIO/SIGURG proc/pgrp */
#define F_SETOWN	6		/* set SIGIO/SIGURG proc/pgrp */

#define	F_GETLK		7		/* get record locking information */
#define	F_SETLK		8		/* set record locking information */
#define	F_SETLKW	9		/* F_SETLK; wait if blocked */

/* file descriptor flags (F_GETFD, F_SETFD) */
#define	FD_CLOEXEC	1		/* close-on-exec flag */

/* record locking flags (F_GETLK, F_SETLK, F_SETLKW) */
#define	F_RDLCK		1		/* shared or read lock */
#define	F_UNLCK		2		/* unlock */
#define	F_WRLCK		3		/* exclusive or write lock */
#ifdef KERNEL
#define	F_WAIT		0x010		/* Wait until lock is granted */
#define	F_FLOCK		0x020	 	/* Use flock(2) semantics for lock */
#define	F_POSIX		0x040	 	/* Use POSIX semantics for lock */
#endif


#define	MAX_NON_LFS	((1UL<<31) - 1)




struct vfs_dir_entry {
    char name[1024];
    struct stat stats;
};
typedef struct vfs_dir_entry dir_entry_t;




typedef struct file {
	struct file *f_next;
    struct file_ops *f_ops;
    u32 f_type;
    u32 f_flags;
    u32 f_curr_pos;
    u32 f_end_pos;
    u32 f_refcnt;
    struct vnode *f_vnode;
} file_t;

#define FTYPE_VNODE		0x01
#define FTYPE_SOCKET	0x02
#define FTYPE_TTY		0x03

typedef struct mount_point {
	struct mount_point_operations *mp_ops;
	u32 mp_flags;
	char  path_prefix[MAX_PREFIX_LEN];
	dev_t *dev;
	void *fsdata;
	struct vnode *vnode;
	struct linked_list list;
} mount_t;


typedef struct mount_point_operations {
	int (*lookup)(struct vnode *pv, const char *path, struct vnode **new);
    int (*open)(struct vnode *pv, const char *path, int mode, file_t **pfile);
    int (*create)(struct vnode *pv, const char *path, int mode, file_t **pfile);
    int (*stat)(struct vnode *pv, const char *path, state_t *buf);
    int (*statfs)(struct vnode *pv, struct statfs *sf);
    int (*sync)(struct vnode *v);
    int (*delete)(struct vnode *pv, const char *path);
} mount_ops_t;


typedef struct file_ops {
    int (*read)(file_t *fp, void *buf, u32 nbytes);
    int (*write)(file_t *fp, void *buf, u32 nbytes);
    int (*fstat)(file_t *fp, struct stat *buf);
    int (*seek)(file_t *fp, off_t pos, int whence);
    int (*close)(file_t *fp);
    int (*ioctl)(file_t *fp, u32 cmd, caddr_t data);
} file_ops_t;



typedef struct vnode
{
	struct vnode *v_next;
	u32 v_device;  // parent device
	u32 v_refcnt;    // times used
	u32 v_mode;    // mode (file, directory, etc..)
	u32 v_id;      // vnode number
	u32 v_size;    // total size
	u32 v_uid;
	u32 v_gid;
	struct vnode *v_mounted; // Mount covered vnode, '..' case
	struct mount_point *v_mp; // mount point -> mount root vnode
	u32 v_atime;
	u32 v_ctime;
	u32 v_mtime;
	void *v_data;   // for file system specific info;
} vnode_t;




typedef struct file_system_ops {
	int (*format)(dev_t *dev);
	int (*mount)(vnode_t *mp);
} fs_ops_t;


typedef struct file_system {
	struct file_system_ops *ops;
	char name[16];
	struct linked_list fs_list;
} fs_t;


typedef struct { int32_t val[2]; } fsid_t;	/* file system id type */

/*
 * file system statistics
 */

#define MFSNAMELEN	16	/* length of fs type name, including null */
#define	MNAMELEN	90	/* length of buffer for returned name */

struct statfs {
	short	f_type;			/* filesystem type number */
	short	f_flags;		/* copy of mount flags */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	uid_t	f_owner;		/* user that mounted the filesystem */
	long	f_spare[4];		/* spare for later */
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
};





extern int register_filesystem(const char *fsname, fs_ops_t *fs_ops);
extern int vfs_mount(const char *devname, const char *prefix, const char *fstype, vnode_t **root);

extern vnode_t *vfs_alloc_vnode(void);
extern void vfs_free_file(file_t *fp);
extern int vfs_free_vnode(vnode_t *v);
extern char *vfs_get_name(const char *path);
extern int vfs_put_name(char *name);
extern file_t *vfs_alloc_file();
extern vnode_t *vfs_get_rootv();


extern int vfs_create(vnode_t *pv, const char *path, int mode, file_t **file);
extern int vfs_close(file_t *file);
extern int vfs_read(file_t *file, void *buf, unsigned long len);
extern int vfs_write(file_t *file, void *buf, unsigned long len);
extern int vfs_seek(file_t *file, off_t pos, int whence);
extern int vfs_open(vnode_t *pv, const char *path, int mode, file_t **file);
extern int kopen(const char *path, int flag, int mode);
extern int kread(int fd, void *buf, int len);
extern int kwrite(int fd, void *buf, int len);
extern int kclose(int fd);
extern int kfstat(int fd, struct stat *st);
extern int kstat(const char *path, struct stat *buf);
extern off_t klseek(int fd, off_t offset, int whence);
extern int kioctl(int fd, int cmd, caddr_t data);
extern int kgetdents(int fd, u8 *buf, int size);
extern int kfcntl(int fd, int cmd, int arg);
extern int kfchdir(int fd);
extern int kchdir(char *path);
extern int kdup2(int oldfd, int newfd);
extern int kmkdir(char *path, int mode);
extern int kgetpid(void);
extern int kgetppid(void);
extern int kgetuid(void);
extern int kgeteuid(void);
extern int kgetgid(void);
extern int kgetegid(void);
extern int kgetpgid(int pid);
extern int ksetegid(int egid);
extern int kseteuid(int euid);
extern int kgetpgrp(void);
extern int ksetpgid(int pid, int pgid);
extern int ktcgetpgrp(int fd);
extern ssize_t kwritev(int fd, const struct iovec *uiov, int iovcnt);
extern ssize_t kreadv(int fd, const struct iovec *uiov, int iovcnt);
extern int kgetdirentries(int fd, char *buf, int nbytes, long basep);
extern int kstatfs(char *path, struct statfs *sf);
extern int kfstatfs(int fd, struct statfs *sf);
extern int kutimes(char *path, struct timeval *tval);
extern int kfsync(int fd);
extern int kftruncate(int fd, off_t len);
extern int kfchmod(int fd, mode_t mode);
extern int kchmod(char *path, mode_t mode);

#endif /* SHAUN_VFS_H_ */
