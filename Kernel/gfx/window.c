/*
 * window.c
 *
 *  Created on: Sep 29, 2015
 *      Author: root
 */

#include <kernel/kernel.h>
#include <kernel/gfx/gfx.h>
#include <kernel/kthread.h>
#include <kernel/gfx/bitmap.h>
#include <string.h>
int kwindow_create(char *title, rect_t *rect, int flag)
{
	int ret;
	frame_t *fm;
	kprocess_t *proc = current->host;
	bmp_t *pen;
	int len;
	u32 bk_color;
	int x, y;
	u8 *s = (u8 *)title;
	rect_t rct;
	ret = kframe_create(0, rect, flag);

	if (ret < 0)
		return ret;


	//create title bar

	fm = kframe_get_framep(proc, ret);
	if (fm == NULL) {
		BUG();
	}

	pen = fm->pen;

	pen->bmp_ops->rect_fill(pen, 0, 0,  rect->width,  20, 0x000000);

	pen->font_color = 0xFFFFFF;
	bk_color = pen->back_color;
	pen->back_color = 0x00000000;
	len = strlen(title);

	//len *= fm->font->width;

	//frame_text(fm, (rect->width - len) >> 1 , 2 , title);

	x = (rect->width - len * fm->font->width) >> 1;
	y = 2;

	while (*s && len--) {
		draw_ascii(fm->pen, fm->font, s, x, y, fm->pen->font_color );
		x += fm->font->width;
		s++;
	}

	rct.x1 = x ;
	rct.y1 = y ;
	rct.width =  fm->font->width;
	rct.height = fm->font->height;

	frame_update(fm->parent, &rct);
	video_refresh();
	pen->back_color = bk_color;
	pen->font_color = 0x000000;
	return ret;

}
