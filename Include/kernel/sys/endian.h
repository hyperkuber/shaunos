/*
 * endian.h
 *
 *  Created on: Jun 21, 2013
 *      Author: root
 */

#ifndef SHAUN_ENDIAN_H_
#define SHAUN_ENDIAN_H_


#include <kernel/types.h>

#define __swab32(x) \
	 ( \
		((u32)( \
		(((u32)(x) & (u32)0x000000ffUL) << 24) | \
		(((u32)(x) & (u32)0x0000ff00UL) << 8 ) | \
		(((u32)(x) & (u32)0x00ff0000UL) >> 8 ) | \
		(((u32)(x) & (u32)0xff000000UL) >> 24) )) \
	)


#define __swab16(x)	\
		(((((u16)(x) & (u16)0x00ffU) << 8) | \
		(((u16)(x) & (u16)0xff00U) >> 8)))





#define htons(x) __swab16(x)
#define htonl(x) __swab32(x)
#define ntohs(x) __swab16(x)
#define ntohl(x) __swab32(x)


#define	NTOHL(x)	(x) = ntohl(x)
#define	NTOHS(x)	(x) = ntohs(x)
#define	HTONL(x)	(x) = htonl(x)
#define	HTONS(x)	(x) = htons(x)

#endif /* SHAUN_ENDIAN_H_ */
