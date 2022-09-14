/*
 *  check_sum.c
 *
 *  Created on: Jul 1, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/types.h>
#include <kernel/mbuf.h>
#include <kernel/kernel.h>

#define ADDCARRY(x)	((x) > 65535 ? (x) -= 65535 : (x))
#define REDUCE {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);}


int in_cksum(struct mbuf *m, int len)
{
	u16	*w;
	int sum = 0;
	int mlen = 0;
	int byte_swapped = 0;

	union {
		char c[2];
		u16	s;
	} s_util;

	union {
		u16	s[2];
		long	l;
	} l_util;


	for (; m && len; m=m->m_next){
		if (m->m_len == 0)
			continue;
		w = mtod(m, u16 *);
		if (mlen == -1){
			s_util.c[1] = *(char *)w;
			sum += s_util.s;
			w = (u16 *)((char *)w + 1);
			mlen = m->m_len -1;
			len--;
		} else
			mlen = m->m_len;

		if (len < mlen){
			mlen = len;
		}

		len -= mlen;
		if ((1 & (int)w) && (mlen > 0)) {
			REDUCE;
			sum <<=8;
			s_util.c[0] = *(u8 *)w;
			w = (u16 *)((u8 *)w + 1);
			mlen--;
			byte_swapped = 1;
		}


		while ((mlen -= 32) >= 0){
			sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3]; sum += w[4];
			sum += w[5]; sum += w[6]; sum += w[7]; sum += w[8]; sum += w[9];
			sum += w[10]; sum += w[11]; sum += w[12]; sum += w[13]; sum += w[14];
			sum += w[15];

			w += 16;
		}
		mlen += 32;

		while ((mlen -= 8) >= 0){
			sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
			w += 4;
		}

		mlen += 8;
		if (mlen == 0 && byte_swapped == 0)
			continue;
		REDUCE;

		while ((mlen -= 2) >= 0){
			sum += *w++;
		}

		if (byte_swapped){
			REDUCE;
			sum <<= 8;
			byte_swapped = 0;
			if (mlen == -1){
				s_util.c[1] = *(char *)w;
				sum += s_util.s;
				mlen = 0;
			} else
				mlen = -1;
		} else if (mlen == -1)
			s_util.c[0] = *(char *)w;

	}

	if (len)
		panic("chsum: out of data");

	if (mlen == -1){
		s_util.c[1] = 0;
		sum += s_util.s;
	}

	REDUCE;
	return (~sum & 0xFFFF);
}




















