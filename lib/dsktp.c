/*
 * dsktp.c
 *
 *  Created on: May 22, 2014
 *      Author: root
 */

#include <sys/syscall.h>
#include <sys/window.h>
#include <sys/editbox.h>
#include <sys/label.h>

int EventHandler(u32 msgid, WPARAM wParam, LPARAM lParam)
{

	int key;
	switch (msgid) {
	case FM_KEYDOWN:
		key = wParam;
		break;
	case FM_KEYUP:
		break;
	default:
		break;
	}
	return 0;
}


int main(int argc, char *argv[])
{

	rect_t rect;
	struct window *wnd;
	struct editbox *eb;
	struct label *lb;
	rect.x1 = 300;
	rect.y1 = 100;
	rect.height = 300;
	rect.width = 400;

	wnd = window_create("window1", &rect, 0);
	wnd->event_handler = EventHandler;
	rect.x1 = 20;
	rect.y1 = 30;
	rect.width = 40;
	rect.height = 20;
	lb = label_create(&wnd->fm, &rect, LABEL_DEFAULT_FLAG);
	lb->set_text(lb, "Name:");
	rect.x1 = 61;
	rect.width = 300;
	eb = editbox_create(&wnd->fm, &rect, EDITBOX_DEFAULT_FLAG);
	


	window_run(wnd);
	return 0;
}
