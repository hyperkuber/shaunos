/*
 * kframe.c
 *
 *  Created on: May 24, 2014
 *      Author: Shaun
 */

#include <kernel/kernel.h>
#include <kernel/gfx/gfx.h>
#include <kernel/kthread.h>
#include <kernel/malloc.h>

int kframe_id_get(kprocess_t *proc)
{
	int ret;

	ret = get_unused_id(&proc->p_fmid_obj);
	if (ret < 0)
		return -1;

	/*
	 *
	 */
	ret += 1024;
	return ret;
}

int kframe_id_put(kprocess_t *proc, int id)
{
	if (id < 1024)
		return -EINVAL;

	id -= 1024;

	put_id(&proc->p_fmid_obj, id);
	return 0;
}


int kframe_id_attach(kprocess_t *proc, int id, frame_t *fm)
{
	if (id < 1024)
		return -EINVAL;

	u32 **fm_addr = proc->frame;

	id -= 1024;
	if (!fm_addr || fm_addr[id] != NULL)
		return -EINVAL;

	fm_addr[id] = (u32 *)fm;

	return 0;
}

int kframe_id_dettach(kprocess_t *proc, int id)
{
	if (id < 1024)
		return -EINVAL;

	id -= 1024;
	u32 **addr = proc->frame;
	if (!addr || addr[id] == NULL)
		return -EINVAL;

	addr[id] = NULL;
	return 0;
}

frame_t *kframe_get_framep(kprocess_t *proc, int id)
{
	if (id < 1024)
		return NULL;

	if (id == 0)
		return NULL;

	id -= 1024;

	if (!proc->frame)
		return NULL;
	return (frame_t *)(proc->frame[id]);
}

int kframe_get_message(MSG *pmsg)
{
	kprocess_t *proc;
	proc = current->host;
	int x;
	fm_msg_t *pfm;

again:

	pfm = proc->p_msgq;
	if (pfm != NULL) {
		proc->p_msgq = pfm->fm_next;
		proc->p_nrmsgs--;
		pmsg->lParam = pfm->fm_param[1];
		pmsg->wParam = pfm->fm_param[0];
		pmsg->message = pfm->fm_msgid;
		pmsg->time = pfm->fm_time;
		kfree(pfm);

	} else {
		wait_queue_head_t wq;
		local_irq_save(&x);
		list_init(&wq.waiter);
		list_del(&current->thread_list);

		local_irq_restore(x);

		list_add_tail(&wq.waiter, &current->thread_list);

		//frame_add_waitq(current->active_frame, &wq);

		schedule();
		goto again;
	}
	return 0;
}

extern frame_t *fm_main;
int kframe_create(int fm_id, rect_t *rect, int flag)
{
	frame_t *fm;
	frame_t *pfm;
	bmp_t *pen = NULL;
	int ret = 0;
	int fmid;
	kprocess_t *proc;

	proc = current->host;

	pfm = kframe_get_framep(proc, fm_id);
	if (pfm == NULL) {
		pen = frame_create_pen(fm_main, rect);
		if (pen == NULL)
			return -ENOBUFS;

		fm = frame_create(fm_main, rect, pen);
		if (fm == NULL) {
			frame_delete_pen(pen);
			return -ENOBUFS;
		}

		frame_insert(fm_main, fm);


	} else {

		fm = frame_create(pfm, rect, NULL);
		if (fm == NULL)
			return -ENOBUFS;

		pen = frame_create_compatible_pen(pfm, rect);
		if (pen == NULL) {
			ret = -ENOBUFS;
			goto end;
		}

		if (flag & FM_BG_WHITE){
			pen->back_color = 0xFFFFFF;
			frame_pen_refresh(pfm, pen);
		}


		frame_select_pen(fm, pen);
		//pen->bmp_ops->rect(pfm->pen, rect->x1, rect->y1, rect->x1 + rect->width, rect->y1 + rect->height, pen->font_color );
		if (flag & FM_BORDER_EDGE) {
			pen->bmp_ops->rect(pfm->pen, rect->x1, rect->y1, rect->x1 + rect->width, rect->y1 + rect->height, pen->font_color );
		}

	}



	fmid = kframe_id_get(proc);
	if (fmid < 0) {
		frame_delete_pen(pen);
		ret = -ENFILE;
		goto end;
	}

	fm->thread = current;
	fm->fm_owner = proc;

	ret = kframe_id_attach(proc, fmid, fm);
	if (ret < 0)
		goto end1;

	return fmid;
end1:
	kframe_id_put(proc, fmid);
end:
	frame_destroy(fm);
	return ret;
}


int kframe_draw_char(int fmid, rect_t *rect, int c)
{
	frame_t *fm;
	kprocess_t *proc;

	if (fmid < 0)
		return -EINVAL;

	if (!rect)
		return -EINVAL;

	proc = current->host;
	if (!proc)
		return -ENOSYS;

	fm = kframe_get_framep(proc, fmid);
	if (fm == NULL)
		return -EINVAL;

	frame_draw_char(fm, rect->x1,  rect->y1, c);
	video_refresh();
	return 0;
}

int kframe_draw_string(int fm_id, rect_t *rect, char *str)
{
	frame_t *fr;
	kprocess_t *proc;
	if (fm_id < 1024 || !str)
		return -EINVAL;

	proc = current->host;
	if (!proc)
		return -ENOSYS;

	fr = kframe_get_framep(proc, fm_id);
	if (fr == NULL)
		return -EINVAL;

	frame_text(fr, rect->x1, rect->y1, (const char *)str);
	video_refresh();
	return 0;
}

int kframe_text(int fm_id, char *str)
{
	frame_t *fr;
	kprocess_t *proc;
	if (fm_id < 1024 || !str)
		return -EINVAL;

	proc = current->host;
	if (!proc)
		return -ENOSYS;

	fr = kframe_get_framep(proc, fm_id);
	if (fr == NULL)
		return -EINVAL;

	frame_text(fr, 0, 0, (const char *)str);

	return 0;
}

int kframe_show(int fm_id)
{
	frame_t *fr;
	kprocess_t *proc;
	if (fm_id < 1024 )
		return -EINVAL;

	proc = current->host;
	if (!proc)
		return -ENOSYS;

	fr = kframe_get_framep(proc, fm_id);
	if (fr == NULL)
		return -EINVAL;

	frame_show(fr);

	return 0;
}
