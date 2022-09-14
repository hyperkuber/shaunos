/*
 * bitmap.c
 *
 *  Created on: Apr 3, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2014 Shaun Yuan
 */

#include <kernel/gfx/gfx.h>
#include <kernel/gfx/bitmap.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/vfs.h>
#include <kernel/errno.h>
#include <kernel/mm.h>
#include <kernel/assert.h>



void bmp_to_rgb_buffer(char *buf, bmp_t *bmp)
{
	int padding;
	int bpl;
	int psw;
	u32 bufpos;
	u32 newpos;
	int x, y;
	u8 *newbuf = bmp->data;
	padding = 0;
	bpl = bmp->width * (bmp->bpp >> 3);
	while ((bpl + padding) % 4 != 0)
		padding++;

	psw = bpl + padding;

	switch (bmp->bpp) {
	case 8:
		return;
	case 16:
		for (y=0; y<bmp->height; y++) {
			for (x=0; x<bpl; x+=2) {
				newpos = (y*bmp->width *2) + x;
				bufpos = (bmp->height-y-1)*psw + x;
				*(u16 *)&newbuf[newpos] = *(u16 *)&buf[bufpos];
			}
		}
		break;
	case 24:
		for (y=0; y<bmp->height; y++) {
			for (x=0; x<bpl; x+=3) {
				newpos = (y*bmp->width *3) + x;
				bufpos = (bmp->height-y-1)*psw + x;
				newbuf[newpos] = buf[bufpos];
				newbuf[newpos+1] = buf[bufpos+1];
				newbuf[newpos+2] = buf[bufpos+2];
			}
		}
		break;
	case 32:
		for (y=0; y<bmp->height; y++) {
			for (x=0; x<bpl; x+=4) {
				newpos = (y*bmp->width *4) + x;
				bufpos = (bmp->height-y-1)*psw + x;
				*(int *)&newbuf[newpos] = *(int *)&buf[bufpos];
			}
		}
		break;
	}
}



bmp_t *
load_bmp(char *file)
{
	file_t *fp;
	vnode_t *rootv;
	int ret;
	char *buf;
	bmp_header_t *bmp_header;
	bmp_info_header_t *bmp_info;
	bmp_t *bmp;
	char *p;

	if (!file)
		return NULL;

	rootv = vfs_get_rootv();
	if (rootv == NULL)
		return NULL;

	ret = vfs_open(rootv, (const char *)file, O_RDONLY, &fp);
	if (ret < 0)
		return NULL;

	buf = kzalloc(fp->f_vnode->v_size);
	assert(buf != NULL);

	ret = vfs_read(fp, buf, fp->f_vnode->v_size);
	if (ret < 0) {
		LOG_ERROR("load file %s failed");
		kfree(buf);
		return NULL;
	}

	vfs_close(fp);

	bmp_header = (bmp_header_t *)buf;
	if (bmp_header->type != 0x4d42){
		LOG_ERROR("invalid bmp header");
		kfree(buf);
		return NULL;
	}
	bmp_info = (bmp_info_header_t *)(bmp_header+1);

	if (bmp_info->bit_cnt != 24) {
		LOG_ERROR("unsupported bmp bits");
		kfree(buf);
		return NULL;
	}

	bmp = (bmp_t *)kzalloc(sizeof(bmp_t));
	assert(bmp != NULL);

	bmp->bpp = bmp_info->bit_cnt;
	bmp->width = bmp_info->width;
	bmp->height = bmp_info->height;
	bmp->data = (u8 *)kzalloc(bmp->width * bmp->height * bmp->bpp);

	switch (bmp->bpp) {
	case 8:
		bmp->bmp_ops = &bitmap_ops8;
		break;
	case 16:
		bmp->bmp_ops = &bitmap_ops16;
		break;
	case 24:
		bmp->bmp_ops = &bitmap_ops24;
		break;
	case 32:
		bmp->bmp_ops = &bitmap_ops32;
		break;
	}

	p = buf + bmp_header->bits_offset;

	bmp_to_rgb_buffer(p, bmp);

	kfree(buf);
	return bmp;
}








