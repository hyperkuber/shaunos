/*
 * uio.h
 *
 *  Created on: Oct 15, 2013
 *      Author: root
 */

#ifndef SHAUN_UIO_H_
#define SHAUN_UIO_H_



#include <kernel/types.h>

struct iovec {
	char *iov_base;
	size_t iov_len;
};

enum uio_rw {
	UIO_READ, UIO_WRITE
};

enum uio_seg {
	UIO_USERSPACE,
	UIO_SYSSPACE,
	UIO_USERISPACE
};

struct uio {
	struct iovec *uio_iov;
	int uio_iovcnt;
	off_t uio_offset;
	int uio_resid;
	enum uio_seg uio_segflg;
	enum uio_rw uio_rw;
};

/*
 * Limits
 */
#define UIO_MAXIOV	1024		/* max 1K of iov's */
#define UIO_SMALLIOV	8		/* 8 on stack, else malloc */

extern int
uiomove(caddr_t cp, int n, struct uio *uio);
#endif /* SHAUN_UIO_H_ */
