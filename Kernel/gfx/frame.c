/*
 * frame.c
 *
 *  Created on: Apr 3, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2014 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/gfx/gfx.h>
#include <kernel/gfx/bitmap.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/mouse.h>
#include <kernel/mm.h>
#include <kernel/kthread.h>



frame_t *fm_main = NULL;
frame_t *fm_mouse = NULL;
frame_t *fm_dsktp = NULL;
frame_t *fm_act = NULL;
frame_t *fm_last_act = NULL;

#define FM_MSG_MAX	8

int frame_add_waitq(frame_t *fm, wait_queue_head_t *wq)
{
	fm->wq = wq;
	return 0;
}

frame_t *
frame_create(frame_t *parent, struct rect *rect, bmp_t *pen)
{

	struct frame *fr = kzalloc(sizeof(struct frame));
	assert(fr != NULL);
	fr->parent = parent;

	if (parent) {
		fr->rect.x1 =  rect->x1;
		fr->rect.y1 =  rect->y1;
		fr->rect.width = rect->width > parent->rect.width ? parent->rect.width : rect->width;
		fr->rect.height = rect->height > parent->rect.height ? parent->rect.height : rect->height;

		fr->font = parent->font;
		fr->raster = parent->raster;
		fr->fm_owner = parent->fm_owner;

	} else {
		fr->rect = *rect;
		fr->font = fm_main->font;
		fr->raster = fm_main->raster;
		fr->fm_owner = current->host;
	}
	if (pen != NULL) {
		fr->pen = pen;
	}

	return fr;
}


bmp_t *
frame_create_pen(frame_t *fm, rect_t *rect)
{
	bmp_t *pen;
	u32 i;
	int w,h;
	u8 *p;

	pen = kzalloc(sizeof(bmp_t));
	if (pen == NULL)
		return NULL;


	w = rect->width > fm->rect.width ? fm->rect.width : rect->width;
	h = rect->height > fm->rect.height ? fm->rect.height : rect->height;

	pen->bpp = fm->pen->bpp;
	pen->bmp_ops = fm->pen->bmp_ops;
	pen->height = h;
	pen->width = w;
	pen->back_color = 0xd6d7d9;
	pen->size = w * h * (pen->bpp>>3);
	pen->data = kzalloc(pen->size);
	if (pen->data == NULL)
		goto end;
	//pen->data += (rect->y1 * pfm->rect.width  + rect->x1) * (pen->bpp >> 3);

	pen->font_color = 0;

	p = pen->data;

	switch (pen->bpp){
	case 24:
		for(i=0; i<w*h; i++){
			(*(int *)p) = pen->back_color;
			p += 3;
		}
		break;
	case 32:
		for(i=0; i<w*h; i++){
			(*(int *)p) = pen->back_color;
			p += 4;
		}
		break;
	default:
		memset(pen->data, pen->back_color, pen->size);
		break;
	}

	return pen;
end:
	kfree(pen);
	return NULL;
}

void frame_delete_pen(bmp_t *pen)
{
	if (pen) {
		if (pen->data)
			kfree(pen->data);
		kfree(pen);
	}
}

bmp_t *
frame_create_compatible_pen(frame_t *pfm, rect_t *rect)
{
	bmp_t *pen;
	u32 i, j;
	int w,h;
	u8 *p;
	pen = kzalloc(sizeof(bmp_t));
	if (pen == NULL)
		return NULL;


	w = rect->width > pfm->rect.width ? pfm->rect.width : rect->width;
	h = rect->height > pfm->rect.height ? pfm->rect.height : rect->height;

	pen->bpp = pfm->pen->bpp;
	pen->bmp_ops = pfm->pen->bmp_ops;
	pen->height = h;
	pen->width = w;
	//pen->back_color = 0xFFFFFF;	//white
	pen->back_color = pfm->pen->back_color;
	pen->size = w * h * (pen->bpp>>3);
	//pen->data = kzalloc(pen->size);
	pen->data = pfm->pen->data;
	pen->data += ((rect->y1 * pfm->rect.width  + rect->x1) * (pen->bpp >> 3));

	pen->font_color = 0x000000;	//black

	p = pen->data;

	switch (pen->bpp){
	case 24:
		for(i=0; i<h*3; i+=3){
			for (j=0; j<w*3; j+=3) {
				(*(int *)&p[i*pfm->rect.width + j]) = pen->back_color;
				//p += 3;
			}


		}
		break;
	case 32:
		for(i=0; i<w*h; i++){
			(*(int *)p) = pen->back_color;
			p += 4;
		}
		break;
	default:
		memset(pen->data, pen->back_color, pen->size);
		break;
	}
	//pen->data = pfm->pen->data;
	return pen;
}


void frame_pen_refresh(frame_t *fm, bmp_t *pen)
{
	int i, j;
	u8 *p;
	p = pen->data;
	int w, h;

	w = pen->width;
	h = pen->height;

	switch(pen->bpp) {
	case 24:
		for(i=0; i<h*3; i+=3){
			for (j=0; j<w*3; j+=3) {
				(*(int *)&p[i*fm->rect.width + j]) = pen->back_color;
				//p += 3;
			}


		}
		break;
	case 32:
		for(i=0; i<w*h; i++){
			(*(int *)p) = pen->back_color;
			p += 4;
		}
		break;
	default:
		memset(pen->data, pen->back_color, pen->size);
		break;

	}

}

int frame_select_pen(frame_t *fm, bmp_t *pen)
{

	if (fm == NULL || pen == NULL)
		return -EINVAL;

	if (fm->pen) {
		//free pen?
	}

	fm->pen = pen;
	return 0;
}

int frame_destroy(frame_t *fm)
{
	if (fm->pen) {
		kfree(fm->pen);
	}
	kfree((void *)fm);
	return 0;
}

void frame_show(frame_t *frame)
{
	if (frame->pen == NULL)
		return;

	if (frame->parent == NULL)
		return;

	bitblt(frame->parent->pen, frame->rect.x1, frame->rect.y1, frame->pen, 0, 0,
			frame->rect.width, frame->rect.height, frame->raster);
}

void frame_update(frame_t *frame, rect_t *rect)
{
	if (frame->pen == NULL)
		return;
	if (frame->parent == NULL)
		return;

	if (rect == NULL)
		return;




	bitblt(frame->parent->pen, frame->rect.x1 + rect->x1, frame->rect.y1 + rect->y1,
			frame->pen, rect->x1, rect->y1, rect->width, rect->height, frame->raster);
}

void frame_render(frame_t *frame, rect_t *rect)
{
	int w,h;
	if (frame->pen == NULL)
		return;
	if (frame->parent == NULL)
		return;

	if (rect == NULL)
		return;
	frame->rect.x1 += rect->x1;
	frame->rect.y1 += rect->y1;

	if (frame->rect.x1 > fm_dsktp->rect.width)
		frame->rect.x1--;
	if (frame->rect.x1 < 0)
		frame->rect.x1 = 0;
	if (frame->rect.y1 < 0)
		frame->rect.y1 = 0;


	if (frame->rect.x1 + rect->width > fm_dsktp->rect.width)
		w = fm_dsktp->rect.width - frame->rect.x1 -1;
	else
		w = rect->width;
	if (frame->rect.y1 + rect->height > fm_dsktp->rect.height)
		h = fm_dsktp->rect.height - frame->rect.y1 -1;
	else
		h = rect->height;


	bitblt(frame->parent->pen, frame->rect.x1, frame->rect.y1,
			frame->pen, 0, 0, w, h, frame->raster);
}

void frame_text(frame_t *fm, int x, int y, const char *str)
{
	int len = strlen(str);
	rect_t rct;

	if (!str)
		return;

	if ((len << 3) > fm->rect.width)
		len = (fm->rect.width >> 3)-4;

	u8 *s = (u8 *)str;

	u32 width = fm->pen->width;

	fm->pen->width = fm->parent->pen->width;

	//fm->pen->bmp_ops->rect_fill(fm->pen, 0, 0,  fm->rect.width,  fm->rect.y1+fm->rect.height, fm->pen->back_color);

	while (*s && len--) {
		draw_ascii(fm->pen, fm->font, s, x, y, fm->pen->font_color );
		x += fm->font->width;
		s++;
	}


	fm->pen->width = width;
	rct.x1 = x + fm->rect.x1;
	rct.y1 = y + fm->rect.y1;
	rct.width =  fm->font->width;
	rct.height = fm->font->height;

	frame_update(fm->parent, &rct);

}

void frame_draw_char(frame_t *fm, int x, int y, int c)
{

	rect_t rct;
	u32 width = fm->pen->width;

	fm->pen->width = fm->parent->pen->width;
	draw_ascii(fm->pen, fm->font, (u8 *)&c, x, y, fm->pen->font_color );
	fm->pen->width = width;
	rct.x1 = x + fm->rect.x1;
	rct.y1 = y + fm->rect.y1;
	rct.width =  fm->font->width;
	rct.height = fm->font->height;

	frame_update(fm->parent, &rct);


}


void frame_tip(frame_t *frame, char *tip, int loc)
{
	int xc = frame->rect.width-20;
	int yc = 20;
	int len = strlen(tip);
//	u32 col = frame->pen->bmp_ops->point(frame->pen, xc, yc);
//	frame->pen->bmp_ops->circle(frame->pen, 20, xc, yc, 1, col);
	draw_string(frame->pen, (u8*)tip, len, xc-8, yc, 0xFFFFFFFF);
	frame_show(frame);
}


frame_t *
frame_launch(frame_t *parent, char *bitmap)
{
	bmp_t *pen;
	rect_t rect;
	frame_t *frame = NULL;
	pen = load_bmp(bitmap);
	if (pen == NULL)
		return NULL;
	rect.x1 = 0;
	rect.y1 = 0;
	rect.height = pen->height;
	rect.width = pen->width;

	frame = frame_create(parent, &rect, pen);

	return frame;
}

int frame_insert(frame_t *parent, frame_t *fr)
{
	frame_t **p;
	p = &parent->next;
//	int width = parent->rect.width;
//	int x,y;
//	x = y = 0;
	while (*p) {

//		x += (*p)->rect.width;
//		x += 10;
//		if (x + fr->rect.width > width) {
//			x = 0;
//			y += (*p)->rect.height;
//			y += 10;
//			if (y > parent->rect.height)
//				y = 0;
//
//		}
		p = &(*p)->next;
	}

	*p = fr;
//	fr->parent = parent;
//	fr->next = NULL;
//	fr->raster = parent->raster;

//	fr->rect.x1 = parent->rect.x1 + x;
//	fr->rect.y1 = parent->rect.y1 + y;
	return 0;
}

void frame_init(bmp_t *pen)
{
//	frame_t *com_frm;
//	frame_t *music_frm;
	bmp_t *desktop;
	struct rect rct_main;
//	draw_string(pen, (u8 *)"Start", 5, 50, 60, 0xFFFFFF);
//	video_refresh();
//	if ((avatar = load_bmp("/root/shaun.bmp")) == NULL)
//		return ;
//
//	bitblt(pen, 860, 40, avatar, 0, 0, avatar->width, avatar->height, SRCCOPY);
//	draw_string(pen, (u8 *)"Shaun", 5, 815, 60, 0xFFFFFF);
//	//bitblt(pen, 100, 60, mouse_cursor, 0, 0, mouse_cursor->width, mouse_cursor->height, SRCPAINT);
//	video_refresh();

	rct_main.x1 = 0;
	rct_main.y1 = 0;
	rct_main.width = pen->width;
	rct_main.height = pen->height;
	desktop = kmalloc(sizeof(bmp_t));
	assert(desktop != NULL);

	desktop->width = pen->width;
	desktop->height = pen->height;
	desktop->bpp = pen->bpp;
	desktop->bmp_ops = pen->bmp_ops;
	desktop->data = (u8 *)alloc_pages(ALLOC_ZEROED|ALLOC_NORMAL, 10);
	memcpy((void *)desktop->data, (void *)pen->data, pen->size);

	fm_main = frame_create(NULL, &rct_main, pen);
	fm_main->raster = SRCCOPY;
	fm_main->font = &font_8x16;

	fm_dsktp = frame_create(fm_main, &rct_main, desktop);
	if (fm_dsktp == NULL)
		return;

	fm_dsktp->parent = fm_main;

	frame_show(fm_dsktp);
	video_refresh();
	fm_mouse = frame_launch(fm_main, "/root/arrow.bmp");
	if (fm_mouse == NULL)
		return;
	fm_mouse->raster = SRCINVERT;
	fm_mouse->rect.x1 = rct_main.x1;
	fm_mouse->rect.y1 = rct_main.y1;
	fm_mouse->parent = fm_main;

	frame_show(fm_mouse);
	video_refresh();
}

frame_t *
frame_from_pos(frame_t *parent, pos_t *pos)
{
	frame_t *pfm = NULL;
	for (pfm=parent->next; pfm != NULL; pfm = pfm->next) {
		if (pos->x1 > pfm->rect.x1 && pos->x1 < pfm->rect.x1 + pfm->rect.width &&
				pos->y1 > pfm->rect.y1 && pos->y1 < pfm->rect.y1 + pfm->rect.height) {
			pos->x1 -= pfm->rect.x1;
			pos->y1 -= pfm->rect.y1;
			return frame_from_pos(pfm, pos);
		}
	}
	return parent;
}

int frame_is_overlapped(frame_t *frame, rect_t *rect)
{
	struct r {
		s32 x1, y1;	//top
		s32 x2, y2;	//bottom
	};

	struct r r1, r2;
	r1.x1 = frame->rect.x1;

	r1.y1 = frame->rect.y1;
	r1.x2 = frame->rect.x1 + frame->rect.width;
	r1.y2 = frame->rect.y1 + frame->rect.height;

	r2.x1 = rect->x1;
	r2.y1 = rect->y1;
	r2.x2 = rect->x1 + rect->width;
	r2.y2 = rect->y1 + rect->height;

	return r1.x1 < r2.x2 && r2.x1 < r1.x2 && r1.y1 < r2.y2 && r2.y1 < r1.y2;
}

//rect_t *frame_get_cliped(framt_t *frame, rect_t *rect)
//{
//
//}

frame_t *
frame_lookup_damaged(frame_t *frame, rect_t *rect)
{
	frame_t *pos = frame->next;
	while (pos) {
		if (frame_is_overlapped(pos, rect))
			return pos;
		pos = pos->next;
	}
	return NULL;
}
int frame_get_clipped(frame_t *frame, rect_t *rect, rect_t *clp_rect)
{
	struct r {
		s32 x1, y1;
		s32 x2, y2;
	};
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define MIN(a, b)	((a) > (b) ? (b) : (a))

	struct r r1, r2, rc;
	r1.x1 = frame->rect.x1;
	r1.y1 = frame->rect.y1;
	r1.x2 = frame->rect.x1 + frame->rect.width;
	r1.y2 = frame->rect.y1 + frame->rect.height;

	r2.x1 = rect->x1;
	r2.y1 = rect->y1;
	r2.x2 = rect->x1 + rect->width;
	r2.y2 = rect->y1 + rect->height;

	rc.x1 = MAX(r1.x1, r2.x1);
	rc.y1 = MAX(r1.y1, r2.y1);

	rc.x2 = MIN(r1.x2, r2.x2);
	rc.y2 = MIN(r1.y2, r2.y2);

	if (clp_rect) {
		clp_rect->x1 = rc.x1;
		clp_rect->y1 = rc.y1;
		clp_rect->width = rc.x2 - rc.x1;
		clp_rect->height = rc.y2 - rc.y1;
	}
	return 0;
}
/*
 *
 * it is really a tough work.
 * i don't wanna touch it any more.
 *
 */

void frame_redraw_damaged(frame_t *frame, rect_t *rect)
{
	frame_t *damaged_fm = NULL;
	rect_t new_rect = {0,0,0,0};
	rect_t clprect = {0,0,0,0};
	rect_t new_rect2 = {0,0,0,0};
	new_rect = *rect;
again:
	damaged_fm = frame_lookup_damaged(frame, &new_rect);
	if (damaged_fm) {
		frame_get_clipped(damaged_fm, &new_rect, &clprect);

		if (clprect.x1 != new_rect.x1 && clprect.y1 != new_rect.y1) {
			clprect.x1 = new_rect.x1 + new_rect.width - clprect.width;
			clprect.y1 = new_rect.y1;
			clprect.height = new_rect.height;
			frame_redraw_damaged(frame, &clprect);
			clprect.x1 = new_rect.x1;
			clprect.y1 = new_rect.y1;
			clprect.width = new_rect.width - clprect.width;
			clprect.height = new_rect.height;
			frame_redraw_damaged(frame, &clprect);
			//return;
			goto next;
		}

		if (clprect.width == new_rect.width) {

			if (clprect.y1 == new_rect.y1) {
				new_rect.height -= clprect.height;
				new_rect.y1 += clprect.height;
			}
			else {
				if (new_rect.y1 < clprect.y1) {
					new_rect2.x1 = clprect.x1;
					new_rect2.y1 = clprect.y1 + clprect.height +1;
					new_rect2.width  =clprect.width;
					new_rect2.height = new_rect.height - clprect.height - (clprect.y1 - new_rect.y1);
					frame_redraw_damaged(frame, &new_rect2);
				}

				new_rect.height -= clprect.height;
			}

		}
		else if (clprect.height == new_rect.height) {
			//new_rect.width -= clprect.width;
			if (clprect.x1 == new_rect.x1) {
				new_rect.width -= clprect.width;
				new_rect.x1 += clprect.width;
			}

			else {
				//
				if (new_rect.x1 < clprect.x1) {
					 new_rect2.x1 = clprect.x1 + clprect.width +1;
					 new_rect2.y1 = clprect.y1;
					 new_rect2.width = new_rect.width - clprect.width - (clprect.x1 - new_rect.x1);
					 new_rect2.height = clprect.height;
					 frame_redraw_damaged(frame, &new_rect2);
				}
				if (new_rect.x1 > clprect.x1) {

				}
				//the other case
				new_rect.width -= clprect.width;

			}
		} else {
			if (clprect.x1 == new_rect.x1) {
				clprect.y1 = new_rect.y1;
				clprect.height = new_rect.height;
				frame_redraw_damaged(frame, &clprect);

				clprect.x1 += clprect.width;
				clprect.width = new_rect.width - clprect.width;
				frame_redraw_damaged(frame, &clprect);

				new_rect.width -= clprect.width;
				//return;
				goto next;
			} else {
				clprect.height = new_rect.height;
				frame_redraw_damaged(frame, &clprect);;
				clprect.x1 = new_rect.x1;
				clprect.y1 = new_rect.y1;
				clprect.height = new_rect.height;
				clprect.width = new_rect.width - clprect.width;
				frame_redraw_damaged(frame, &clprect);
				new_rect.width -= clprect.width;
				//return;
				goto next;
			}

		}


//		if (clprect.width < new_rect.width) {
//			if (clprect.x1 == new_rect.x1 && clprect.height == new_rect.height) {
//				new_rect.x1 += clprect.width;
//				new_rect.width -= clprect.width;
//			}
//			else {
////				clprect.x1 = new_rect.x1;
////				clprect.y1 = new_rect.y1;
////				clprect.height = new_rect.height;
////				frame_redraw_damaged(frame, &clprect);
////				clprect.x1 += clprect.width;
////				clprect.width = new_rect.width - clprect.width;
////				frame_redraw_damaged(frame, &clprect);
////				return;
//			}
//
//
//		}
//
//		if (clprect.height < new_rect.height) {
//			if (clprect.y1 == new_rect.y1)
//				new_rect.y1 += clprect.height;
//			new_rect.height -= clprect.height;
//		}


//		//frame_update only takes relative position
		clprect.x1 -= damaged_fm->rect.x1;
		clprect.y1 -= damaged_fm->rect.y1;
		frame_update(damaged_fm, &clprect);

		clprect.x1 += damaged_fm->rect.x1;
		clprect.y1 += damaged_fm->rect.y1;

		//finished?
		if (clprect.height == new_rect.height && clprect.width == new_rect.width &&
				clprect.x1 == new_rect.x1 && clprect.y1 == new_rect.y1)
			return;
//		if (new_rect.height == 0 || new_rect.width == 0)
//			return;

		goto again;
	}
	/*
	 * desktop is special frame, which is excluded in main frame.
	 * after all damaged rects has been done, we focus on it again.
	 */
next:
	if (frame_is_overlapped(fm_dsktp, &new_rect)) {
		frame_get_clipped(fm_dsktp, &new_rect, &clprect);
		if (clprect.width < new_rect.width) {
			if (clprect.x1 == new_rect.x1)
				new_rect.x1 += clprect.width;
			new_rect.width -= clprect.width;
		}

		if (clprect.height < new_rect.height) {
			if (clprect.y1 == new_rect.y1)
				new_rect.y1 += clprect.height;
			new_rect.height -= clprect.height;
		}

		//frame_update only takes relative position
		clprect.x1 -= fm_dsktp->rect.x1;
		clprect.y1 -= fm_dsktp->rect.y1;
		frame_update(fm_dsktp, &clprect);

		clprect.x1 += fm_dsktp->rect.x1;
		clprect.y1 += fm_dsktp->rect.y1;

		//finished?
		if (clprect.height == new_rect.height && clprect.width == new_rect.width &&
				clprect.x1 == new_rect.x1 && clprect.y1 == new_rect.y1)
			return;

		goto next;
	}

}

int frame_sendmsg(frame_t *frame, u32 msgid, u32 p1, u32 p2)
{
	fm_msg_t *fm_msg = NULL;
	fm_msg_t **p;
	if (!frame)
		return -EINVAL;

	fm_msg = (fm_msg_t *)kzalloc(sizeof(fm_msg_t));
	if (fm_msg == NULL)
		return -ENOBUFS;

	fm_msg->fm_sender = NULL;
	fm_msg->fm_receiver = frame->fm_owner;
	fm_msg->fm_frame = frame;
	fm_msg->fm_msgid = msgid;
	fm_msg->fm_param[0] = p1;
	fm_msg->fm_param[1] = p2;
	fm_msg->fm_time = 0;

	if (frame->fm_owner) {
		if (frame->fm_owner->p_nrmsgs >= 64) {
			kfree(fm_msg);
			//hang frame
			//LOG_INFO("Frame message num exceeded.");
			return -1;
		}
		p = &frame->fm_owner->p_msgq;
	}

	else {
		//created by kernel thread?
		kfree(fm_msg);
		return 0;
	}


	while (*p)
		p = &((*p)->fm_next);

	*p = fm_msg;
	frame->fm_owner->p_nrmsgs++;

	pt_wakeup(frame->thread);
	//wakeup_waiters(frame->wq);
	//frame->wq = NULL;
	return 0;
}


#define ABS(x) ((x) > 0 ? (x) : (-x))

void frame_mouse_event(struct mouse_input *val)
{
	struct mouse_input *pmsdata = val;
	pos_t pos;
	rect_t new_rect;
	u32 p1;
	frame_show(fm_mouse);
	if (pmsdata->ms_dx || pmsdata->ms_dy) {

		fm_mouse->rect.x1 += pmsdata->ms_dx;
		fm_mouse->rect.y1 += -pmsdata->ms_dy;

		if (fm_mouse->rect.x1 < 0)
			fm_mouse->rect.x1 = 0;
		if (fm_mouse->rect.y1 < 0)
			fm_mouse->rect.y1 = 0;

		if (fm_mouse->rect.x1 >= fm_main->rect.width)
			fm_mouse->rect.x1 = fm_main->rect.width-fm_mouse->rect.width;
		if (fm_mouse->rect.y1 >= fm_main->rect.height)
			fm_mouse->rect.y1 = fm_main->rect.height-fm_mouse->rect.height;

		if (fm_act == NULL)
			goto end;

		if (pmsdata->ms_state & MS_LEFT_BUTTON) {

			if (fm_act == fm_main)
				goto end;



			//save old pos
			pos.x1 = fm_act->rect.x1;
			pos.y1 = fm_act->rect.y1;

			frame_show(fm_act);

			new_rect.x1 =  pmsdata->ms_dx;
			new_rect.y1 =  - pmsdata->ms_dy;
			new_rect.height = fm_act->rect.height;
			new_rect.width = fm_act->rect.width;

			frame_render(fm_act, &new_rect);

			//redraw old position
			if (pmsdata->ms_dy < 0) {
				new_rect.x1 = pos.x1;
				new_rect.y1 = pos.y1;
				new_rect.height = ABS(pmsdata->ms_dy);
				new_rect.width = fm_act->rect.width +1;
				frame_redraw_damaged(fm_mouse->parent, &new_rect);

				if (pmsdata->ms_dx > 0) {
					new_rect.x1 = pos.x1;
					new_rect.y1 = pos.y1 + ABS(pmsdata->ms_dy);
					new_rect.width = pmsdata->ms_dx;
					new_rect.height = fm_act->rect.height - ABS(pmsdata->ms_dy);
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}

				if (pmsdata->ms_dx < 0) {
					new_rect.x1 = pos.x1 + fm_act->rect.width - ABS(pmsdata->ms_dx)+1;
					new_rect.y1 = pos.y1 + ABS(pmsdata->ms_dy);
					new_rect.height = fm_act->rect.height - ABS(pmsdata->ms_dy);
					new_rect.width = ABS(pmsdata->ms_dx) + 1;
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}

			}
			if (pmsdata->ms_dy == 0) {
				if (pmsdata->ms_dx > 0) {
					new_rect.x1 = pos.x1;
					new_rect.y1 = pos.y1;
					new_rect.width = pmsdata->ms_dx;
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}
				if (pmsdata->ms_dx < 0) {
					new_rect.x1 = pos.x1 + fm_act->rect.width - ABS(pmsdata->ms_dx)+1;
					new_rect.y1 = pos.y1;
					new_rect.width = ABS(pmsdata->ms_dx);
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}

			}

//			if (pmsdata->ms_dx == 0) {
//				if (pmsdata->ms_dy > 0) {
//					new_rect.x1 = pos.x1;
//					new_rect.y1 = pos.y1;
//					new_rect.width = fm_act->rect.width;
//					new_rect.height = pmsdata->ms_dy;
//
//					frame_redraw_damaged(fm_mouse->parent, &new_rect);
//				}
//				if (pmsdata->ms_dy < 0) {
//
//
//
//					if (new_rect.x1 < clprect.x1) {
//						 new_rect2.x1 = clprect.x1 + clprect.width;
//						 new_rect2.y1 = clprect.y1;
//						 new_rect2.width = new_rect.width - clprect.width - (clprect.x1 - new_rect.x1);
//						 new_rect2.height = clprect.height;
//						 frame_redraw_damaged(frame, &new_rect2);
//					}
//				}
//					new_rect.x1 = pos.x1;
//					new_rect.y1 = pos.y1 + fm_act->rect.height - ABS(pmsdata->ms_dy);
//					new_rect.height = ABS(pmsdata->ms_dy);
//					new_rect.width = fm_act->rect.width;
//
//					frame_redraw_damaged(fm_mouse->parent, &new_rect);
//
//				}
//
//			}


			if (pmsdata->ms_dy > 0) {

				new_rect.x1 = pos.x1;
				new_rect.y1 = pos.y1 + fm_act->rect.height - pmsdata->ms_dy;
				new_rect.height = pmsdata->ms_dy;
				new_rect.width = fm_act->rect.width+1;
				frame_redraw_damaged(fm_mouse->parent, &new_rect);


				if (pmsdata->ms_dx > 0) {
					new_rect.x1 = pos.x1;
					new_rect.y1 = pos.y1;
					new_rect.width = pmsdata->ms_dx;
					new_rect.height = fm_act->rect.height - pmsdata->ms_dy;
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}

				if (pmsdata->ms_dx < 0) {
					new_rect.x1 = pos.x1 + fm_act->rect.width - ABS(pmsdata->ms_dx)+1;
					new_rect.y1 = pos.y1;
					new_rect.height = fm_act->rect.height - pmsdata->ms_dy;
					new_rect.width = ABS(pmsdata->ms_dx);
					frame_redraw_damaged(fm_mouse->parent, &new_rect);
				}

			}
//			frame_sendmsg(fm_act, FM_MOUSEMOVE, fm_mouse->rect.x1 << 16 | fm_mouse->rect.y1,
//					pmsdata->ms_wheel << 16 | pmsdata->ms_state);

			//mouse drag
//			frame_sendmsg(fm_act, FM_MOUSEMOVE, fm_mouse->rect.x1 << 16 | fm_mouse->rect.y1,
//								pmsdata->ms_wheel << 16 | FM_LBUTTONDOWN);

//			fm_act->rect.x1 += pmsdata->ms_dx;
//			fm_act->rect.y1 += pmsdata->ms_dy;

			p1 = 0;
			p1 = (u16)pmsdata->ms_dy;
			p1 |= (u32)(pmsdata->ms_dx << 16);

			frame_sendmsg(fm_act, FM_MOUSEMOVE, p1,
								pmsdata->ms_wheel << 16 | FM_LBUTTONDOWN);

			goto end;
		}

		//mouse hover
		frame_sendmsg(fm_act, FM_MOUSEMOVE, fm_mouse->rect.x1 << 16 | fm_mouse->rect.y1,
							pmsdata->ms_wheel << 16 | pmsdata->ms_state);

	}

	if (pmsdata->ms_state & MS_LEFT_BUTTON) {
		pos.x1 = fm_mouse->rect.x1;
		pos.y1 = fm_mouse->rect.y1;
		fm_act = frame_from_pos(fm_mouse->parent, &pos);
		if (fm_act == fm_main)
			goto end;

//		if (fm_last_act == fm_act)
//			goto end;

		frame_show(fm_act);
		fm_last_act = fm_act;

		frame_sendmsg(fm_act, FM_LBUTTONDOWN, fm_mouse->rect.x1 << 16 | fm_mouse->rect.y1,
							pmsdata->ms_wheel << 16 | FM_LBUTTONDOWN);

	}

	if (pmsdata->ms_state & MS_RIGHT_BUTTON) {
		frame_sendmsg(fm_act, FM_RBUTTONDOWN, fm_mouse->rect.x1 << 16 | fm_mouse->rect.y1,
									pmsdata->ms_wheel << 16 | FM_RBUTTONDOWN);
	}
	//current->active_frame = fm_act;

end:
	frame_show(fm_mouse);
	video_refresh();
}


void frame_kbd_event(struct kbd_input *key)
{

	if (fm_act) {

		if (fm_act == fm_main)
			return;

		if (key->kbd_flag >> 8 & KB_DOWN) {
			if (key->kbd_keycode == 0x3F00)	//F5 pressed
				video_refresh();

			//LOG_DEBG("key down:%x", key->kbd_keycode);
			frame_sendmsg(fm_act, FM_KEYDOWN, key->kbd_keycode, key->kbd_flag);
		} else {
			//LOG_DEBG("key up:%x", key->kbd_keycode);
			frame_sendmsg(fm_act, FM_KEYUP, key->kbd_keycode, key->kbd_flag);
		}
	}

}

