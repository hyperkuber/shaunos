/*
 * font.h
 *
 *  Created on: Apr 4, 2014
 *      Author: root
 */

#ifndef SHAUN_FONT_H_
#define SHAUN_FONT_H_


typedef struct font {
	u8 *name;
	u32 width;
	u32 height;
	u8 *data;
	u32 language;
	u32 type;
} font_t;



//extern font_t font_8x8;
extern font_t font_8x16;

#endif /* SHAUN_FONT_H_ */
