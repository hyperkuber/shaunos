/*
 * video.h
 *
 *  Created on: Apr 3, 2014
 *      Author: root
 */

#ifndef SHAUN_VIDEO_H_
#define SHAUN_VIDEO_H_


#include <kernel/types.h>

typedef struct _screen {
	u32 phy_addr;
	u8 *fb;
	u32 xres;
	u32 yres;
	u32 bpp;
	u32 mode;
	u32 bpl;
}screen_t;

extern  screen_t g_screen;

extern void video_refresh(void);

#endif /* SHAUN_VIDEO_H_ */
