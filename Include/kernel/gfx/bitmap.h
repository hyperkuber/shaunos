/*
 * bitmap.h
 *
 *  Created on: Apr 2, 2014
 *      Author: root
 */

#ifndef SHAUN_BITMAP_H_
#define SHAUN_BITMAP_H_

#include <kernel/types.h>


struct bitmap_ops;

typedef struct bitmap {
	u32 width;
	u32 height;
	u32 size;
	u32 bpp;
	u32 mask_color;
	u32 back_color;
	u8 *data;
	u32 font_color;
	struct bitmap_ops *bmp_ops;
}bmp_t;
typedef struct bitmap pen_t;


typedef struct bitmap_ops {
	void (*pixel)(bmp_t *bmp, s32 x, s32 y, s32 color);
	void (*line)(bmp_t *bmp, s32 x1, s32 y1, s32 x2, s32 y2, s32 color);
	void (*rect)(bmp_t *bmp, s32 x1, s32 y1, s32 x2, s32 y2, s32 color);
	void (*rect_fill)(bmp_t *bmp, s32 x1, s32 y1, s32 x2, s32 y2, s32 color);
	void (*circle)(bmp_t *bmp, s32 r, s32 xc, s32 yc, s32 fill, s32 color);
	void (*clear)(bmp_t *bmp);
	u32 (*point)(bmp_t *bmp, s32 x, s32 y);
} bmp_ops_t;


struct _bmp_header {
	unsigned short type;
	unsigned long size;
	unsigned int resevd;
	unsigned long bits_offset;
} __attribute__((__packed__));
typedef struct _bmp_header bmp_header_t;


typedef struct _bmp_info_header {
	unsigned long size;
	unsigned long width;
	unsigned long height;
	unsigned short planes;
	unsigned short bit_cnt;
	unsigned long compression;
	unsigned long size_img;
	unsigned long xpels_per_meter;
	unsigned long ypels_per_meter;
	unsigned long clr_used;
	unsigned long clr_important;
} bmp_info_header_t;


extern struct bitmap_ops bitmap_ops24;
extern struct bitmap_ops bitmap_ops32;
extern struct bitmap_ops bitmap_ops8;
extern struct bitmap_ops bitmap_ops16;
extern bmp_t *
load_bmp(char *file);

#endif /* SHAUN_BITMAP_H_ */
