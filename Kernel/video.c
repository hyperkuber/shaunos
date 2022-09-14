/*
 * video.c
 *
 *  Created on: Feb 18, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 *
 *  a simple vesa card driver, based on
 *  VBE3.0 Spec, and  only support 32bits
 *  color mode, to be upgraded
 */
#include <kernel/kernel.h>
#include <kernel/int86.h>
#include <kernel/vesa.h>
#include <kernel/mm.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>
#include <kernel/page.h>
#include <kernel/console.h>
#include <kernel/int86.h>
#include <kernel/cpu.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>
#include <kernel/gfx/gfx.h>


unsigned long desired_xres = 1024;
unsigned long desired_yres = 768;
extern struct con_obj curr_con;
unsigned int enable_lfb = 1;


screen_t g_screen;
void put_pixel(unsigned int x, unsigned int y, unsigned int color);
void (*bank_switch)(void);
void bankground_init(char *path);
void video_refresh(void);
unsigned long bank_shift;
unsigned long cur_bank;

extern vnode_t *ext2_rootv;

int video_init()
{

	unsigned short *pmode;
	vesa_info_t *vinfo = (vesa_info_t *)kmalloc(sizeof(vesa_info_t));
	assert(vinfo != NULL);
	vesa_mode_info_t *mode  = kmalloc(sizeof(*mode));
	assert(mode != NULL);


	int retval = vesa_get_info(vinfo);
	if (retval == -1){
		LOG_INFO("Unknow video controller, kernel start failed, only support vesa");
		while (1);
	}


	for (pmode = vinfo->modes; *pmode != 0xFFFF; pmode++){
		retval = vesa_get_mode_info(*pmode, mode);
		if (retval == -1){
			LOG_ERROR("get mode:%d info failed", *pmode);
			goto END;
		}
		if (mode->bits_per_pixel == 32 || mode->bits_per_pixel == 24){
			if (mode->memory_model != 6 ||
					!(mode->mode_attr & 0x01) ||	//mode supported in hardware
					!(mode->mode_attr & 0x08) ||	//color mode
					!(mode->mode_attr & 0x10) ||	//graphic mode
					!(mode->mode_attr & 0x80) ||	//Linear frame buffer is available
					mode->number_of_planes == 0)
				continue;

			if (mode->xres == desired_xres && mode->yres == desired_yres){
				LOG_INFO("set mode(%x) %dx%d, fb:%x",*pmode, mode->xres, mode->yres, mode->phys_base_ptr);
				g_screen.bpp = mode->bits_per_pixel;
				if (enable_lfb){
					g_screen.phy_addr =  mode->phys_base_ptr;
					if (g_screen.phy_addr == 0)
						continue;
					//Note, vbe spec 3.0 say we should set bytes per scan line to LinBytesPerScanLine
					//in 1.2 and later, we set it to xres
					g_screen.bpl = mode->bytes_per_scan_line;// mode->xres;
				} else {
					//banked mode
					g_screen.phy_addr =  VA(mode->bank_a_segment << 4);
					g_screen.bpl = mode->bytes_per_scan_line;
				}

				g_screen.mode = *pmode;
				g_screen.xres = mode->xres;
				g_screen.yres = mode->yres;
				//do not use this, use int86 0x400f
				bank_switch = (void (*)(void))PA(mode->pos_func_ptr);
				cur_bank = -1;
				bank_shift = 0;
				while (((unsigned)64 >> bank_shift) != mode->bank_granularity)
					bank_shift++;
				break;
			}
		}

	}

	LOG_INFO("g_screen.fb:%x, bpl:%d win_func:%x", g_screen.phy_addr, g_screen.bpl, (unsigned long)bank_switch);
	LOG_INFO("red pos:%d redmask:%d blue pos:%d blue mask:%d green pos:%d green mask:%d",
			mode->red_field_pos,mode->red_mask_size, mode->blue_field_pos, mode->blue_mask_size,
			mode->green_field_pos, mode->green_mask_size);


	map_page(g_screen.phy_addr, g_screen.phy_addr, get_cr3(), PAGE_PRE|PAGE_RW|PAGE_PSE|PAGE_GLOBAL, NULL);
	//to enable lfb mode, bit 14 of mode number must be set
	retval = vesa_set_mode((unsigned short)g_screen.mode | 0x4000);
	if (retval < 0){
		LOG_ERROR("set mode failed");
		goto END;
	}

	bankground_init("/root/bk.bmp");

	setup_gfx(&g_screen);

END:
	kfree((void *)vinfo);
	kfree(mode);
	return retval;

}

void set_bank(int bank)
{
	if (bank == cur_bank)
		return;
	cur_bank = bank;
	bank <<= bank_shift;
#ifdef DIRECT_BANKING
	asm volatile ("movw	$0,	%%bx\n\t"
			"movw	%w0,	%%dx\n\t"
			::"r"(bank):);

	bank_switch();

	asm volatile ("movw	$1,	%%bx\n\t"
			"movw	%w0,	%%dx\n\t"
			::"r"(bank):);
	bank_switch();
#else

	int86_regs_t regs;
	regs.x.ax = 0x4f05;
	regs.x.bx = 0;
	regs.x.dx = bank;
	int86(0x10, &regs, &regs);
	regs.x.ax = 0x4f05;
	regs.x.bx = 1;
	regs.x.dx = bank;
	int86(0x10, &regs, &regs);
#endif


}



/*
 * this is slow, direct put pixel in video card memory,
 * so, it needs no refresh
 */
void put_pixel(unsigned int x, unsigned int y, unsigned int color)
{

	unsigned long addr;
	//Note:do not use xres here, use bytes_per_scan_line
	addr = (((g_screen.bpl * y) + x * (g_screen.bpp >> 3)));
	if (!enable_lfb){
		set_bank(addr >> 16);
	}

	switch(g_screen.bpp)
	{
		case 8: /* 8-Bit */
			*((unsigned char *)addr) = (unsigned char)color;
			break;

		case 16: /* 16-Bit */
			*((unsigned short *)addr) = (unsigned short)color;
			break;

		case 24: /* 24-Bit (32-bit, no alpha channel) */
			//*((unsigned int *)addr) = color & 0x00ffffff;
			*((unsigned char *)g_screen.phy_addr + addr) = color & 0xff;
			*((unsigned char *)g_screen.phy_addr + addr+1) = (color >> 8) & 0xff;
			*((unsigned char *)g_screen.phy_addr + addr+2) = (color >> 16) & 0xff;
			break;

		case 32: /* 32-Bit, 1 alpha channel */
			if (enable_lfb)
				*((unsigned int *)((unsigned char *)g_screen.phy_addr + addr)) = color;
			else
				*((unsigned int *)((unsigned char *)g_screen.phy_addr + (addr & 0xFFFF))) = color;

			break;
	}
}

/*
 * put pixel in frame buffer, needs refresh to see what has
 * changed
 */
void put_pixel32(unsigned int x, unsigned int y, unsigned int color)
{
	unsigned long addr;
	addr = ((g_screen.bpl * y) + x * 4);
	if (enable_lfb) {
		*((unsigned int *)(g_screen.fb + addr)) = color;
	} else {
		set_bank(addr >> 16);
		*((unsigned int *)(g_screen.fb + (addr & 0xFFFF))) = color;
	}
}

unsigned int get_pixel32(unsigned int x, unsigned int y)
{
	unsigned long addr = ((g_screen.bpl * y) + x * 4);
	if (enable_lfb){
		return *((unsigned int *)(g_screen.fb + addr));
	} else {
		set_bank(addr >> 16);
		return *((unsigned int *)(g_screen.fb + (addr & 0xFFFF)));
	}
}

void draw_line(int x1, int y1, int x2, int y2, int color)
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
		put_pixel(x1, y1, color);

		for (x1++; x1 <= x2; x1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				y1 += yincr;
			}

			put_pixel(x1, y1, color);
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
		put_pixel(x1, y1, color);

		for (y1++; y1 <= y2; y1++){
			if (d < 0)
				d += eincr;
			else {
				d += neincr;
				x1 += yincr;
			}

			put_pixel(x1, y1, color);
		}

	}

}




void fill_rect(int x1, int y1, int x2, int y2, int color)
{
	int y;
	if(y1 > y2){
		y = y2;
		y2 = y1;
		y1 = y;
	}
	for(y = y1; y < y2 + 1; y++)
		draw_line(x1, y, x2, y, color);
}


void bankground_init(char *path)
{
	file_t *fbk;
	int retval;

	retval = vfs_open(vfs_get_rootv(), path, O_RD, &fbk);
	if (retval < 0){
		LOG_ERROR("open file:%s failed", path);
		while (1);
	}

	unsigned char *bk_buf = (unsigned char *)alloc_pages(ALLOC_NORMAL, 10);
	assert(bk_buf != NULL);

	retval = vfs_read(fbk, (void *)bk_buf, fbk->f_vnode->v_size);
	if (retval < 0){
		LOG_ERROR("read file failed");
		while (1);
	}

	vfs_close(fbk);
	bmp_header_t *bmp_header = (bmp_header_t *)bk_buf;
	if (bmp_header->type != 0x4d42){
		LOG_ERROR("invalid bmp header");
		return;
	}

	bk_buf += bmp_header->bits_offset;
	int j = g_screen.xres * g_screen.yres * 3;
	int x, y;
	y = 0;
	x = g_screen.xres - 1;
	while(y < g_screen.yres)
	{
		while(x >= 0)
		{
			put_pixel(x, y, *((unsigned int *)&bk_buf[j]));
			j -= 3;
			x--;
		}
		x = g_screen.xres - 1;
		y++;
	}

	bk_buf -= bmp_header->bits_offset;

	memcpy((void *)bk_buf, (void *)g_screen.phy_addr, g_screen.bpl * g_screen.yres);
	//test for refresh

	g_screen.fb = bk_buf;
	//fill_rect(0, 0, g_screen.xres-100, g_screen.yres-68, 0xFF0A3B76);
	video_refresh();

	//draw_line(100, 200, 400, 200, 0xFFFF);

}


void video_refresh(void)
{
	memcpy((void *)g_screen.phy_addr, (void *)g_screen.fb, g_screen.bpl * g_screen.yres);
}










