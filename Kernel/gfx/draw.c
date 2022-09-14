/*
 * draw.c
 *
 *  Created on: Apr 2, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/gfx/bitmap.h>


static void inline pixel24(bmp_t *bmp, s32 x, s32 y, s32 color)
{
	if (x > bmp->width || y > bmp->height)
		return;

	u8 *pos = (bmp->data + (y * bmp->width + x) * 3);
	*pos++ = color & 0xFF;
	*pos++ = (color >> 8 & 0xFF);
	*pos = (color >> 16 & 0xFF);
}
static u32 inline point24(bmp_t *bmp, s32 x, s32 y)
{
	if (x > bmp->width || y > bmp->height)
		return 0;

	u32 *pos = (u32 *)(bmp->data + (y * bmp->width + x) * 3);
	return *pos;
}

static void inline pixel32(bmp_t *bmp, s32 x, s32 y, s32 color)
{
	if (x > bmp->width || y > bmp->height)
		return;

	s32 *pos = (s32 *)(bmp->data + (y * bmp->width + x) * 4);
	*pos = color;
}

static u32 inline point32(bmp_t *bmp, s32 x, s32 y)
{
	if (x > bmp->width || y > bmp->height)
		return 0;

	u32 *pos = (u32 *)(bmp->data + (y * bmp->width + x) * 4);
	return *pos;
}

static void inline pixel16(bmp_t *bmp, s32 x, s32 y, s32 color)
{
	if (x > bmp->width || y > bmp->height)
		return;

	u16 *pos = (u16 *)(bmp->data + (y * bmp->width + x) * 2);
	*pos = color & 0xFFFF;
}

static u32 inline point16(bmp_t *bmp, s32 x, s32 y)
{
	if (x > bmp->width || y > bmp->height)
		return 0;

	u16 *pos = (u16 *)(bmp->data + (y * bmp->width + x) * 2);
	return (u32)(*pos);
}

static void inline pixel8(bmp_t *bmp, s32 x, s32 y, s32 color)
{
	if (x > bmp->width || y > bmp->height)
		return;

	u8 *pos = (bmp->data + (y * bmp->width + x));
	*pos = color & 0xFF;
}
static u32 inline point8(bmp_t *bmp, s32 x, s32 y)
{
	if (x > bmp->width || y > bmp->height)
		return 0;

	u8 *pos = (u8 *)(bmp->data + (y * bmp->width + x));
	return (u32)(*pos);
}

void draw_line32(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int d, dx, dy;
	int eincr, neincr;
	int yincr;
	int t;

#define ABS(a)	((a) >= 0 ? (a) : (-a))

	dx = ABS(x2 - x1);
	dy = ABS(y2 - y1);

	if (dy <= dx){
		if (x2 < x1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (y2 > y1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dy - dx;
		eincr = 2*dy;
		neincr = 2*(dy - dx);
		pixel32(bmp, x1, y1, color);

		for (x1++; x1 <= x2; x1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				y1 += yincr;
			}

			pixel32(bmp,x1,y1,color);
		}
	}
	else {
		if (y2 < y1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (x2 > x1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dx - dy;
		eincr = 2*dx;
		neincr = 2 * (dx - dy);
		pixel32(bmp, x1, y1, color);

		for (y1++; y1 <= y2; y1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				x1 += yincr;
			}

			pixel32(bmp, x1, y1, color);
		}

	}

}

/*
 * Bresenham's line algorithm
 */
void draw_line24(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int d, dx, dy;
	int eincr, neincr;
	int yincr;
	int t;

#define ABS(a)	((a) >= 0 ? (a) : (-a))

	dx = ABS(x2 - x1);
	dy = ABS(y2 - y1);

	if (dy <= dx){
		if (x2 < x1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (y2 > y1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dy - dx;
		eincr = 2*dy;
		neincr = 2*(dy - dx);
		pixel24(bmp, x1, y1, color);

		for (x1++; x1 <= x2; x1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				y1 += yincr;
			}

			pixel24(bmp, x1,y1,color);
		}
	}
	else {
		if (y2 < y1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (x2 > x1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dx - dy;
		eincr = 2*dx;
		neincr = 2 * (dx - dy);
		pixel24(bmp, x1, y1, color);

		for (y1++; y1 <= y2; y1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				x1 += yincr;
			}

			pixel24(bmp, x1, y1, color);
		}

	}

}

void draw_line16(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int d, dx, dy;
	int eincr, neincr;
	int yincr;
	int t;

#define ABS(a)	((a) >= 0 ? (a) : (-a))

	dx = ABS(x2 - x1);
	dy = ABS(y2 - y1);

	if (dy <= dx){
		if (x2 < x1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (y2 > y1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dy - dx;
		eincr = 2*dy;
		neincr = 2*(dy - dx);
		pixel16(bmp, x1, y1, color);

		for (x1++; x1 <= x2; x1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				y1 += yincr;
			}

			pixel16(bmp, x1,y1,color);
		}
	}
	else {
		if (y2 < y1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (x2 > x1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dx - dy;
		eincr = 2*dx;
		neincr = 2 * (dx - dy);
		pixel16(bmp, x1, y1, color);

		for (y1++; y1 <= y2; y1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				x1 += yincr;
			}

			pixel16(bmp, x1, y1, color);
		}

	}

}

void draw_line8(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int d, dx, dy;
	int eincr, neincr;
	int yincr;
	int t;

#define ABS(a)	((a) >= 0 ? (a) : (-a))

	dx = ABS(x2 - x1);
	dy = ABS(y2 - y1);

	if (dy <= dx){
		if (x2 < x1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (y2 > y1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dy - dx;
		eincr = 2*dy;
		neincr = 2*(dy - dx);
		pixel8(bmp, x1, y1, color);

		for (x1++; x1 <= x2; x1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				y1 += yincr;
			}

			pixel8(bmp, x1,y1,color);
		}
	}
	else {
		if (y2 < y1){
			t = x2; x2 = x1; x1 = t;
			t = y2; y2 = y1; y1 = t;
		}

		if (x2 > x1)
			yincr = 1;
		else
			yincr = -1;

		d = 2*dx - dy;
		eincr = 2*dx;
		neincr = 2 * (dx - dy);
		pixel8(bmp, x1, y1, color);

		for (y1++; y1 <= y2; y1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				x1 += yincr;
			}

			pixel8(bmp, x1, y1, color);
		}

	}

}


void rect24(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	draw_line24(bmp, x1, y1, x2, y1, color);
	draw_line24(bmp, x1, y2, x2, y2, color);
	draw_line24(bmp, x1, y1, x1, y2, color);
	draw_line24(bmp, x2, y1, x2, y2, color);
}

void fill_rect24(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int y;
	if(y1 > y2){
		y = y2;
		y2 = y1;
		y1 = y;
	}
	for(y = y1; y < y2 + 1; y++)
		draw_line24(bmp, x1, y, x2, y, color);
}


void rect32(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	draw_line32(bmp, x1, y1, x2, y1, color);
	draw_line32(bmp, x1, y2, x2, y2, color);
	draw_line32(bmp, x1, y1, x1, y2, color);
	draw_line32(bmp, x2, y1, x2, y2, color);
}
void fill_rect32(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int y;
	if(y1 > y2){
		y = y2;
		y2 = y1;
		y1 = y;
	}
	for(y = y1; y < y2 + 1; y++)
		draw_line32(bmp, x1, y, x2, y, color);
}

void rect16(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	draw_line16(bmp, x1, y1, x2, y1, color);
	draw_line16(bmp, x1, y2, x2, y2, color);
	draw_line16(bmp, x1, y1, x1, y2, color);
	draw_line16(bmp, x2, y1, x2, y2, color);
}

void fill_rect16(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int y;
	if(y1 > y2){
		y = y2;
		y2 = y1;
		y1 = y;
	}
	for(y = y1; y < y2 + 1; y++)
		draw_line16(bmp, x1, y, x2, y, color);
}

void rect8(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	draw_line8(bmp, x1, y1, x2, y1, color);
	draw_line8(bmp, x1, y2, x2, y2, color);
	draw_line8(bmp, x1, y1, x1, y2, color);
	draw_line8(bmp, x2, y1, x2, y2, color);
}

void fill_rect8(bmp_t *bmp, int x1, int y1, int x2, int y2, int color)
{
	int y;
	if(y1 > y2){
		y = y2;
		y2 = y1;
		y1 = y;
	}
	for(y = y1; y < y2 + 1; y++)
		draw_line8(bmp, x1, y, x2, y, color);
}


static inline void _draw_circle_8(bmp_t *bmp, int xc, int yc, int x, int y,  s32 c)
{
    pixel8(bmp, xc + x, yc + y, c);
    pixel8(bmp, xc - x, yc + y, c);
    pixel8(bmp, xc + x, yc - y, c);
    pixel8(bmp, xc - x, yc - y, c);
    pixel8(bmp, xc + y, yc + x, c);
    pixel8(bmp, xc - y, yc + x, c);
    pixel8(bmp, xc + y, yc - x, c);
    pixel8(bmp, xc - y, yc - x, c);
}

static inline void _draw_circle_16(bmp_t *bmp, int xc, int yc, int x, int y, s32 c)
{
    pixel16(bmp, xc + x, yc + y, c);
    pixel16(bmp, xc - x, yc + y, c);
    pixel16(bmp, xc + x, yc - y, c);
    pixel16(bmp, xc - x, yc - y, c);
    pixel16(bmp, xc + y, yc + x, c);
    pixel16(bmp, xc - y, yc + x, c);
    pixel16(bmp, xc + y, yc - x, c);
    pixel16(bmp, xc - y, yc - x, c);
}
static inline void _draw_circle_32(bmp_t *bmp, int xc, int yc, int x, int y, s32 c)
{
    pixel32(bmp, xc + x, yc + y, c);
    pixel32(bmp, xc - x, yc + y, c);
    pixel32(bmp, xc + x, yc - y, c);
    pixel32(bmp, xc - x, yc - y, c);
    pixel32(bmp, xc + y, yc + x, c);
    pixel32(bmp, xc - y, yc + x, c);
    pixel32(bmp, xc + y, yc - x, c);
    pixel32(bmp, xc - y, yc - x, c);
}

static inline void _draw_circle_24(bmp_t *bmp, int xc, int yc, int x, int y, s32 c)
{
    pixel24(bmp, xc + x, yc + y, c);
    pixel24(bmp, xc - x, yc + y, c);
    pixel24(bmp, xc + x, yc - y, c);
    pixel24(bmp, xc - x, yc - y, c);
    pixel24(bmp, xc + y, yc + x, c);
    pixel24(bmp, xc - y, yc + x, c);
    pixel24(bmp, xc + y, yc - x, c);
    pixel24(bmp, xc - y, yc - x, c);
}

//Bresenham's circle algorithm
void draw_circle8(bmp_t *bmp, s32 r, s32 xc, s32 yc, s32 fill, s32 c)
{
    if (xc + r < 0 || xc - r >= bmp->width ||
            yc + r < 0 || yc - r >= bmp->height) return;

    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;

    if (fill) {
        while (x <= y) {
            for (yi = x; yi <= y; yi ++)
                _draw_circle_8(bmp, xc, yc, x, yi, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x++;
        }
    } else {
        while (x <= y) {
            _draw_circle_8(bmp, xc, yc, x, y, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x ++;
        }
    }
}

void draw_circle16(bmp_t *bmp, s32 r, s32 xc, s32 yc, int fill, s32 c)
{
    if (xc + r < 0 || xc - r >= bmp->width ||
            yc + r < 0 || yc - r >= bmp->height) return;

    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;

    if (fill) {
        while (x <= y) {
            for (yi = x; yi <= y; yi ++)
                _draw_circle_16(bmp, xc, yc, x, yi, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x++;
        }
    } else {
        while (x <= y) {
            _draw_circle_16(bmp, xc, yc, x, y, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x ++;
        }
    }
}

void draw_circle24(bmp_t *bmp, s32 r, s32 xc, s32 yc, int fill, s32 c)
{
    if (xc + r < 0 || xc - r >= bmp->width ||
            yc + r < 0 || yc - r >= bmp->height) return;

    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;

    if (fill) {
        while (x <= y) {
            for (yi = x; yi <= y; yi ++)
                _draw_circle_24(bmp, xc, yc, x, yi, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x++;
        }
    } else {
        while (x <= y) {
            _draw_circle_24(bmp, xc, yc, x, y, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x ++;
        }
    }
}

void draw_circle32(bmp_t *bmp, s32 r, s32 xc, s32 yc, int fill, s32 c)
{
    if (xc + r < 0 || xc - r >= bmp->width ||
            yc + r < 0 || yc - r >= bmp->height) return;

    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;

    if (fill) {
        while (x <= y) {
            for (yi = x; yi <= y; yi ++)
                _draw_circle_32(bmp, xc, yc, x, yi, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y --;
            }
            x++;
        }
    } else {
        while (x <= y) {
            _draw_circle_32(bmp, xc, yc, x, y, c);

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}


 struct bitmap_ops bitmap_ops8 = {
		.pixel = pixel8,
		.line = draw_line8,
		.rect = rect8,
		.rect_fill = fill_rect8,
		.circle = draw_circle8,
		.clear = NULL,
		.point = point8,
};


 struct bitmap_ops bitmap_ops16 = {
		.pixel = pixel16,
		.line = draw_line16,
		.rect = rect16,
		.rect_fill = fill_rect16,
		.circle = draw_circle16,
		.clear = NULL,
		.point = point16,
};

struct bitmap_ops bitmap_ops24 = {
		.pixel = pixel24,
		.line = draw_line24,
		.rect = rect24,
		.rect_fill = fill_rect24,
		.circle = draw_circle24,
		.clear = NULL,
		.point = point24,
};

struct bitmap_ops bitmap_ops32 = {
		.pixel = pixel32,
		.line = draw_line32,
		.rect = rect32,
		.rect_fill = fill_rect32,
		.circle = draw_circle32,
		.clear = NULL,
		.point = point32,
};



