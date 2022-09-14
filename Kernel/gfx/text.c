/*
 * text.c
 *
 *  Created on: Apr 4, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2014 Shaun Yuan
 */

#include <kernel/gfx/bitmap.h>
#include <kernel/gfx/gfx.h>
#include <kernel/gfx/font.h>


void draw_ascii(bmp_t *pen, font_t *font, u8 *ch, int x, int y, int color)
{

	u8 *bits;
	int i, j;
	if (font->width == 8 && font->height == 12)
		bits = font->data + ((*ch) * 12);
	else if (font->width == 8 && font->height == 16)
		bits = font->data + ((*ch) * 16);
	else
		bits = font->data + ((*ch)* 8);

	for (j=0; j<font->height; j++) {

		for (i=0; i<font->width; i++) {
			if ((bits[j] >> (font->width-1-i)) & 1) {
				pen->bmp_ops->pixel(pen, x+i, y+j, color);
			} else {
				pen->bmp_ops->pixel(pen, x+i, y+j, pen->back_color);
			}
		}
	}

}

void draw_string(bmp_t *pen, u8 *str, int len, int x, int y, int color)
{
	font_t *font = &font_8x16;
	u8 *ch;
	while ( (ch = str++) && len--) {
		draw_ascii(pen, font, ch, x, y, color);
		x += font->width;
	}

}

