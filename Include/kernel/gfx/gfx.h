/*
 * gfx.h
 *
 *  Created on: Apr 3, 2014
 *      Author: Shaun Yuan
 *      Copyright (c) 2014 Shaun Yuan
 */

#ifndef SHAUN_GFX_H_
#define SHAUN_GFX_H_



#include <kernel/types.h>
#include <kernel/video.h>
#include <kernel/gfx/bitmap.h>
#include <kernel/gfx/font.h>
#include <kernel/mouse.h>
#include <kernel/kthread.h>
#include <kernel/keyboard.h>
#include <kernel/io_dev.h>

#define FM_BG_BLACK   	(1<<0)
#define FM_BG_BLUE    	(1<<1)
#define FM_BG_GREEN   	(1<<2)
#define FM_BG_RED     	(1<<3)
#define FM_BG_GRAY    	(1<<4)
#define FM_BG_WHITE		(1<<5)
#define FM_BG_PARENT	(1<<6)

#define FM_BORDER_FLAT	(1<<10)
#define FM_BORDER_EDGE	(1<<11)


#define BLACKNESS 0x01
#define DSTINVERT 0x02
#define MERGECOPY 0x03
#define MERGEPAINT 0x04
#define NOTSRCCOPY 0x05
#define NOTSRCERASE 0x06
#define PATCOPY 0x07
#define PATINVERT 0x08
#define PATPAINT 0x09
#define SRCAND 0x0A
#define SRCCOPY 0x0B
#define SRCERASE 0x0C
#define SRCINVERT 0x0D
#define SRCPAINT 0x0E
#define WHITENESS 0x0F

/*
 * all these definitions comes from microsoft
 *
 */
#define FM_NULL                         0x0000
#define FM_CREATE                       0x0001
#define FM_DESTROY                      0x0002
#define FM_MOVE                         0x0003
#define FM_SIZE                         0x0005
#define FM_ACTIVATE                     0x0006
#define FM_SETFOCUS                     0x0007
#define FM_KILLFOCUS                    0x0008
#define FM_ENABLE                       0x000A
#define FM_SETREDRAW                    0x000B
#define FM_SETTEXT                      0x000C
#define FM_GETTEXT                      0x000D
#define FM_GETTEXTLENGTH                0x000E
#define FM_PAINT                        0x000F
#define FM_CLOSE                        0x0010
#define FM_QUERYENDSESSION              0x0011
#define FM_QUERYOPEN                    0x0013
#define FM_ENDSESSION                   0x0016
#define FM_QUIT                         0x0012
#define FM_ERASEBKGND                   0x0014
#define FM_SYSCOLORCHANGE               0x0015
#define FM_SHOWWINDOW                   0x0018
#define FM_WININICHANGE                 0x001A
#define FM_SETTINGCHANGE                FM_WININICHANGE
#define FM_DEVMODECHANGE                0x001B
#define FM_ACTIVATEAPP                  0x001C
#define FM_FONTCHANGE                   0x001D
#define FM_TIMECHANGE                   0x001E
#define FM_CANCELMODE                   0x001F
#define FM_SETCURSOR                    0x0020
#define FM_MOUSEACTIVATE                0x0021
#define FM_CHILDACTIVATE                0x0022
#define FM_QUEUESYNC                    0x0023
#define FM_GETMINMAXINFO                0x0024
#define FM_PAINTICON                    0x0026
#define FM_ICONERASEBKGND               0x0027
#define FM_NEXTDLGCTL                   0x0028
#define FM_SPOOLERSTATUS                0x002A
#define FM_DRAWITEM                     0x002B
#define FM_MEASUREITEM                  0x002C
#define FM_DELETEITEM                   0x002D
#define FM_VKEYTOITEM                   0x002E
#define FM_CHARTOITEM                   0x002F
#define FM_SETFONT                      0x0030
#define FM_GETFONT                      0x0031
#define FM_SETHOTKEY                    0x0032
#define FM_GETHOTKEY                    0x0033
#define FM_QUERYDRAGICON                0x0037
#define FM_COMPAREITEM                  0x0039
#define FM_GETOBJECT                    0x003D
#define FM_COMPACTING                   0x0041
#define FM_COMMNOTIFY                   0x0044  /* no longer suported */
#define FM_WINDOWPOSCHANGING            0x0046
#define FM_WINDOWPOSCHANGED             0x0047
#define FM_POWER                        0x0048
#define FM_COPYDATA                     0x004A
#define FM_CANCELJOURNAL                0x004B
#define FM_NOTIFY                       0x004E
#define FM_INPUTLANGCHANGEREQUEST       0x0050
#define FM_INPUTLANGCHANGE              0x0051
#define FM_TCARD                        0x0052
#define FM_HELP                         0x0053
#define FM_USERCHANGED                  0x0054
#define FM_NOTIFYFORMAT                 0x0055
#define FM_CONTEXTMENU                  0x007B
#define FM_STYLECHANGING                0x007C
#define FM_STYLECHANGED                 0x007D
#define FM_DISPLAYCHANGE                0x007E
#define FM_GETICON                      0x007F
#define FM_SETICON                      0x0080
#define FM_NCCREATE                     0x0081
#define FM_NCDESTROY                    0x0082
#define FM_NCCALCSIZE                   0x0083
#define FM_NCHITTEST                    0x0084
#define FM_NCPAINT                      0x0085
#define FM_NCACTIVATE                   0x0086
#define FM_GETDLGCODE                   0x0087
#define FM_SYNCPAINT                    0x0088
#define FM_NCMOUSEMOVE                  0x00A0
#define FM_NCLBUTTONDOWN                0x00A1
#define FM_NCLBUTTONUP                  0x00A2
#define FM_NCLBUTTONDBLCLK              0x00A3
#define FM_NCRBUTTONDOWN                0x00A4
#define FM_NCRBUTTONUP                  0x00A5
#define FM_NCRBUTTONDBLCLK              0x00A6
#define FM_NCMBUTTONDOWN                0x00A7
#define FM_NCMBUTTONUP                  0x00A8
#define FM_NCMBUTTONDBLCLK              0x00A9
#define FM_NCXBUTTONDOWN                0x00AB
#define FM_NCXBUTTONUP                  0x00AC
#define FM_NCXBUTTONDBLCLK              0x00AD
#define FM_INPUT                        0x00FF
#define FM_KEYFIRST                     0x0100
#define FM_KEYDOWN                      0x0100
#define FM_KEYUP                        0x0101
#define FM_CHAR                         0x0102
#define FM_DEADCHAR                     0x0103
#define FM_SYSKEYDOWN                   0x0104
#define FM_SYSKEYUP                     0x0105
#define FM_SYSCHAR                      0x0106
#define FM_SYSDEADCHAR                  0x0107
#define FM_UNICHAR                      0x0109
//#define FM_KEYLAST                      0x0109
#define UNICODE_NOCHAR                  0xFFFF
#define FM_KEYLAST                      0x0108
#define FM_IME_STARTCOMPOSITION         0x010D
#define FM_IME_ENDCOMPOSITION           0x010E
#define FM_IME_COMPOSITION              0x010F
#define FM_IME_KEYLAST                  0x010F
#define FM_INITDIALOG                   0x0110
#define FM_COMMAND                      0x0111
#define FM_SYSCOMMAND                   0x0112
#define FM_TIMER                        0x0113
#define FM_HSCROLL                      0x0114
#define FM_VSCROLL                      0x0115
#define FM_INITMENU                     0x0116
#define FM_INITMENUPOPUP                0x0117
#define FM_MENUSELECT                   0x011F
#define FM_MENUCHAR                     0x0120
#define FM_ENTERIDLE                    0x0121
#define FM_MENURBUTTONUP                0x0122
#define FM_MENUDRAG                     0x0123
#define FM_MENUGETOBJECT                0x0124
#define FM_UNINITMENUPOPUP              0x0125
#define FM_MENUCOMMAND                  0x0126
#define FM_CHANGEUISTATE                0x0127
#define FM_UPDATEUISTATE                0x0128
#define FM_QUERYUISTATE                 0x0129
#define FM_CTLCOLORMSGBOX               0x0132
#define FM_CTLCOLOREDIT                 0x0133
#define FM_CTLCOLORLISTBOX              0x0134
#define FM_CTLCOLORBTN                  0x0135
#define FM_CTLCOLORDLG                  0x0136
#define FM_CTLCOLORSCROLLBAR            0x0137
#define FM_CTLCOLORSTATIC               0x0138
#define MN_GETHMENU                     0x01E1
#define FM_MOUSEFIRST                   0x0200
#define FM_MOUSEMOVE                    0x0200
#define FM_LBUTTONDOWN                  0x0201
#define FM_LBUTTONUP                    0x0202
#define FM_LBUTTONDBLCLK                0x0203
#define FM_RBUTTONDOWN                  0x0204
#define FM_RBUTTONUP                    0x0205
#define FM_RBUTTONDBLCLK                0x0206
#define FM_MBUTTONDOWN                  0x0207
#define FM_MBUTTONUP                    0x0208
#define FM_MBUTTONDBLCLK                0x0209
#define FM_MOUSEWHEEL                   0x020A
#define FM_XBUTTONDOWN                  0x020B
#define FM_XBUTTONUP                    0x020C
#define FM_XBUTTONDBLCLK                0x020D
#define FM_MOUSELAST                    0x020D
//#define FM_MOUSELAST                    0x020A
//#define FM_MOUSELAST                    0x0209
#define FM_PARENTNOTIFY                 0x0210
#define FM_ENTERMENULOOP                0x0211
#define FM_EXITMENULOOP                 0x0212
#define FM_NEXTMENU                     0x0213
#define FM_SIZING                       0x0214
#define FM_CAPTURECHANGED               0x0215
#define FM_MOVING                       0x0216
#define FM_POWERBROADCAST               0x0218
#define FM_DEVICECHANGE                 0x0219
#define FM_MDICREATE                    0x0220
#define FM_MDIDESTROY                   0x0221
#define FM_MDIACTIVATE                  0x0222
#define FM_MDIRESTORE                   0x0223
#define FM_MDINEXT                      0x0224
#define FM_MDIMAXIMIZE                  0x0225
#define FM_MDITILE                      0x0226
#define FM_MDICASCADE                   0x0227
#define FM_MDIICONARRANGE               0x0228
#define FM_MDIGETACTIVE                 0x0229
#define FM_MDISETMENU                   0x0230
#define FM_ENTERSIZEMOVE                0x0231
#define FM_EXITSIZEMOVE                 0x0232
#define FM_DROPFILES                    0x0233
#define FM_MDIREFRESHMENU               0x0234
#define FM_IME_SETCONTEXT               0x0281
#define FM_IME_NOTIFY                   0x0282
#define FM_IME_CONTROL                  0x0283
#define FM_IME_COMPOSITIONFULL          0x0284
#define FM_IME_SELECT                   0x0285
#define FM_IME_CHAR                     0x0286
#define FM_IME_REQUEST                  0x0288
#define FM_IME_KEYDOWN                  0x0290
#define FM_IME_KEYUP                    0x0291
#define FM_MOUSEHOVER                   0x02A1
#define FM_MOUSELEAVE                   0x02A3
#define FM_NCMOUSEHOVER                 0x02A0
#define FM_NCMOUSELEAVE                 0x02A2
#define FM_WTSSESSION_CHANGE            0x02B1
#define FM_TABLET_FIRST                 0x02c0
#define FM_TABLET_LAST                  0x02df
#define FM_CUT                          0x0300
#define FM_COPY                         0x0301
#define FM_PASTE                        0x0302
#define FM_CLEAR                        0x0303
#define FM_UNDO                         0x0304
#define FM_RENDERFORMAT                 0x0305
#define FM_RENDERALLFORMATS             0x0306
#define FM_DESTROYCLIPBOARD             0x0307
#define FM_DRAWCLIPBOARD                0x0308
#define FM_PAINTCLIPBOARD               0x0309
#define FM_VSCROLLCLIPBOARD             0x030A
#define FM_SIZECLIPBOARD                0x030B
#define FM_ASKCBFORMATNAME              0x030C
#define FM_CHANGECBCHAIN                0x030D
#define FM_HSCROLLCLIPBOARD             0x030E
#define FM_QUERYNEWPALETTE              0x030F
#define FM_PALETTEISCHANGING            0x0310
#define FM_PALETTECHANGED               0x0311
#define FM_HOTKEY                       0x0312
#define FM_PRINT                        0x0317
#define FM_PRINTCLIENT                  0x0318
#define FM_APPCOMMAND                   0x0319
#define FM_THEMECHANGED                 0x031A
#define FM_HANDHELDFIRST                0x0358
#define FM_HANDHELDLAST                 0x035F
#define FM_AFXFIRST                     0x0360
#define FM_AFXLAST                      0x037F
#define FM_PENWINFIRST                  0x0380
#define FM_PENWINLAST                   0x038F
#define FM_APP                          0x8000













typedef struct rect {
	s32 x1, y1;
	s32 width, height;
} rect_t;

typedef struct point {
	s32 x1, y1;
} pos_t;

typedef struct frame {
	struct frame *next;
	kproc_t *fm_owner;
	struct rect rect;
	bmp_t *pen;
	font_t *font;
	struct frame *parent;
	u32 flag;
	u32 raster;
	wait_queue_head_t *wq;
	kthread_t *thread;
} frame_t;

typedef struct frame_msg {
	kproc_t *fm_sender;
	kproc_t *fm_receiver;
	frame_t *fm_frame;
	struct frame_msg *fm_next;
	u32	fm_msgid;
	u32 fm_param[2];
	u32 fm_time;
}fm_msg_t;


typedef struct {
  u32 hwnd;
  u32 message;
  u32 wParam;
  u32 lParam;
  u32 time;
//  POINT pt;
 } MSG, *PMSG;


extern void
setup_gfx(screen_t *g_screen);
extern void frame_init(bmp_t *pen);

extern void draw_string(bmp_t *pen, u8 *str, int len, int x, int y, int color);
extern void draw_ascii(bmp_t *pen, font_t *font, u8 *ch, int x, int y, int color);
extern int
bitblt(bmp_t *dest, int dx, int dy, bmp_t *src, int sx, int sy, int w, int h, int rop);
extern bmp_t *get_current_pen();
extern void frame_mouse_event(struct mouse_input *val);
extern void frame_kbd_event(struct kbd_input *key);
extern void frame_text(frame_t *frame, int x, int y, const char *str);
extern void frame_show(frame_t *frame);
extern int frame_destroy(frame_t *fm);
extern int frame_insert(frame_t *parent, frame_t *fr);
extern frame_t *
frame_create(frame_t *parent, struct rect *rect, bmp_t *pen);
extern bmp_t *
frame_create_compatible_pen(frame_t *pfm, rect_t *rect);
extern void frame_update(frame_t *frame, rect_t *rect);
extern void frame_draw_char(frame_t *fm, int x, int y, int c);

extern void frame_delete_pen(bmp_t *pen);
extern bmp_t *
frame_create_pen(frame_t *fm, rect_t *rect);

extern void frame_pen_refresh(frame_t *fm, bmp_t *pen);
extern int kframe_create(int fm_id, rect_t *rect, int flag);
extern int kframe_text(int fm_id, char *str);
extern int kframe_show(int fm_id);
extern int frame_select_pen(frame_t *fm, bmp_t *pen);
extern frame_t *kframe_get_framep(kprocess_t *proc, int id);

extern int kwindow_create(char *title, rect_t *rect, int flag);
extern int frame_add_waitq(frame_t *fm, wait_queue_head_t *wq);
extern int kframe_get_message(MSG *pmsg);

extern int kframe_draw_char(int fmid, rect_t *rect, int c);
extern int kframe_draw_string(int fm_id, rect_t *rect, char *str);
extern screen_t g_screen;
#endif /* SHAUN_GFX_H_ */
