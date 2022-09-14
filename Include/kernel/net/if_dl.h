/*
 * if_dl.h
 *
 *  Created on: Jun 14, 2013
 *      Author: root
 */

#ifndef SHAUN_IF_DL_H_
#define SHAUN_IF_DL_H_


#include <kernel/types.h>


struct sockaddr_dl {
	u8	sdl_len;
	u8	sdl_family;
	u16	sdl_index;
	u8	sdl_type;
	u8	sdl_nlen;
	u8	sdl_alen;
	u8	sdl_slen;
	s8	sdl_data[12];
};

#define LLADDR(s)	((caddr_t)((s)->sdl_data + (s)->sdl_nlen))


#endif /* SHAUN_IF_DL_H_ */
