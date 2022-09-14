/*
 * uio.c
 *
 *  Created on: Oct 15, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/uio.h>
#include <kernel/kthread.h>
#include <string.h>

int
uiomove(caddr_t cp, int n, struct uio *uio)
{
	struct iovec *iov;
	size_t cnt;
	int error = 0;


	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;

		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}

		if (cnt > n)
			cnt = n;
		switch (uio->uio_segflg) {
		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (uio->uio_rw == UIO_READ)
				error = copy_to_user(iov->iov_base, cp, cnt);
			else
				error = copy_from_user(cp, iov->iov_base, cnt);
			if (error)
				return error;
			break;
		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				memcpy((void *)iov->iov_base, (void *)cp, cnt);
			else
				memcpy((void *)cp, (void *)iov->iov_base, cnt);
			break;
		}

		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp += cnt;
		n -= cnt;
	}

	return error;
}
