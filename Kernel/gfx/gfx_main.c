/*
 * gfx_main.c
 *
 *  Created on: Apr 3, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2014 Shaun Yuan
 */


#include <kernel/video.h>
#include <kernel/gfx/gfx.h>
#include <kernel/gfx/bitmap.h>


bmp_t pen24;
bmp_t pen8;
bmp_t pen16;
bmp_t pen32;
bmp_t *gfx_current_pen;


void setup_gfx(screen_t *g_screen)
{


	switch (g_screen->bpp) {
	case 8:
		pen8.bmp_ops = &bitmap_ops8;
		break;
	case 16:
		pen16.bmp_ops = &bitmap_ops16;
		break;
	case 24:
		pen24.bmp_ops = &bitmap_ops24;
		pen24.bpp = g_screen->bpp;
		pen24.width = g_screen->xres;
		pen24.height = g_screen->yres;
		pen24.data = g_screen->fb;
		pen24.size = g_screen->bpl * g_screen->yres;
		gfx_current_pen = &pen24;
		break;
	case 32:
		pen32.bmp_ops = &bitmap_ops32;
		pen32.bpp = g_screen->bpp;
		pen32.width = g_screen->xres;
		pen32.height = g_screen->yres;
		pen32.data = g_screen->fb;
		pen32.size = g_screen->bpl * g_screen->yres;
		gfx_current_pen = &pen32;
		break;
	}

	frame_init(gfx_current_pen);

}


bmp_t *get_current_pen()
{
	return gfx_current_pen;
}
