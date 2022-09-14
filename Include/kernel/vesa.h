/*
 * vesa.h
 *
 *  Created on: Feb 18, 2013
 *      Author: root
 */

#ifndef SHAUN_VESA_H_
#define SHAUN_VESA_H_



#pragma pack(1)
typedef struct {
	char 			signature [4];
	unsigned short	version;
	char			*oem_string;
	unsigned long	capabilities;
	unsigned short	*modes;
	unsigned short	memory;
	unsigned short	revision;
	char			*vendor_name;
	char			*product_name;
	char			*product_rev;
	char 			reserved [222+256];
} vesa_info_t;

/*
 * VESA mode information
 */
typedef struct {
	unsigned short		mode_attr;
	unsigned char		bank_a_attr;
	unsigned char		bank_b_attr;
	unsigned short		bank_granularity;
	unsigned short		bank_size;
	unsigned short		bank_a_segment;
	unsigned short		bank_b_segment;
	unsigned long		pos_func_ptr;
	unsigned short		bytes_per_scan_line;
	unsigned short		xres;
	unsigned short		yres;
	unsigned char		char_width;
	unsigned char		char_height;
	unsigned char		number_of_planes;
	unsigned char		bits_per_pixel;
	unsigned char		number_of_banks;
	unsigned char		memory_model;
	unsigned char		video_bank_size;
	unsigned char		image_pages;
	unsigned char		reserved_1;
	unsigned char		red_mask_size;
	unsigned char		red_field_pos;
	unsigned char		green_mask_size;
	unsigned char		green_field_pos;
	unsigned char		blue_mask_size;
	unsigned char		blue_field_pos;
	unsigned char		rsvd_mask_size;
	unsigned char		rsvd_field_pos;
	unsigned char		direct_color_info;
	unsigned long 		phys_base_ptr;
	unsigned long 		reserved_2;
	unsigned short		reserved_3;
	unsigned char		reserved [206];
} vesa_mode_info_t;

#pragma pack()

typedef struct {
	unsigned short	xres;
	unsigned short	yres;
	unsigned char	refresh;
} vesa_mode_t;

typedef struct {
	unsigned short	xres;
	unsigned short	yres;
	unsigned char	refresh;
	unsigned short	xsize;
	unsigned short	ysize;
	unsigned long	khz;
	unsigned short	xblank;
	unsigned short	yblank;
	unsigned short	hsync_len;
	unsigned short	vsync_len;
	unsigned short	right_margin;
	unsigned short	lower_margin;
	unsigned short	left_margin;
	unsigned short	upper_margin;
	unsigned char	hsync_positive;
	unsigned char	vsync_positive;
} vesa_extmode_t;

typedef struct {
	unsigned char	version;
	unsigned char	revision;
	unsigned char	manufacturer [4];
	unsigned short	model;
	unsigned long	serial;
	unsigned short	year;
	unsigned short	week;
	unsigned char	monitor [14];	/* Monitor String */
	unsigned char	serial_no [14];	/* Serial Number */
	unsigned char	ascii [14];	/* ? */
	unsigned long	hfmin;		/* hfreq lower limit (Hz) */
	unsigned long	hfmax;		/* hfreq upper limit (Hz) */
	unsigned short	vfmin;		/* vfreq lower limit (Hz) */
	unsigned short	vfmax;		/* vfreq upper limit (Hz) */
	unsigned long	dclkmax;	/* pixelclock upper limit (Hz) */
	unsigned char	gtf;		/* supports GTF */
	unsigned short	input;		/* display type - see VESA_DISP_* */
#define VESA_DISP_DDI			1
#define VESA_DISP_ANA_700_300		2
#define VESA_DISP_ANA_714_286		4
#define VESA_DISP_ANA_1000_400		8
#define VESA_DISP_ANA_700_000		16
#define VESA_DISP_MONO			32
#define VESA_DISP_RGB			64
#define VESA_DISP_MULTI			128
#define VESA_DISP_UNKNOWN		256

	unsigned short	signal;		/* signal Type - see VESA_SIGNAL_* */
#define VESA_SIGNAL_NONE		0
#define VESA_SIGNAL_BLANK_BLANK		1
#define VESA_SIGNAL_SEPARATE		2
#define VESA_SIGNAL_COMPOSITE		4
#define VESA_SIGNAL_SYNC_ON_GREEN	8
#define VESA_SIGNAL_SERRATION_ON	16

	unsigned char	max_x;		/* Maximum horizontal size (cm) */
	unsigned char	max_y;		/* Maximum vertical size (cm) */
	unsigned short	gamma;		/* Gamma - in fractions of 100 */
	unsigned short	dpms;		/* DPMS support - see VESA_DPMS_ */
#define VESA_DPMS_ACTIVE_OFF		1
#define VESA_DPMS_SUSPEND		2
#define VESA_DPMS_STANDBY		4

	unsigned short	misc;		/* Misc flags - see VESA_MISC_* */
#define VESA_MISC_PRIM_COLOR	1
#define VESA_MISC_1ST_DETAIL	2	/* First Detailed Timing is preferred */

	unsigned long	chroma_redx;	/* in fraction of 1024 */
	unsigned long	chroma_greenx;
	unsigned long	chroma_bluex;
	unsigned long	chroma_whitex;
	unsigned long	chroma_redy;
	unsigned long	chroma_greeny;
	unsigned long	chroma_bluey;
	unsigned long	chroma_whitey;
	vesa_mode_t	vesa_modes [17];
	vesa_mode_t	std_modes [8];
	vesa_extmode_t	detailed_modes [24];
} vesa_display_t;

int
vesa_get_info (vesa_info_t *i);

int
vesa_get_mode_info (unsigned short mode, vesa_mode_info_t *i);

int
vesa_set_mode (unsigned short mode);

#endif /* SHAUN_VESA_H_ */
