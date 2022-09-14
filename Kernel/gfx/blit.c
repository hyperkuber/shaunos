/*
 * blit.c
 *
 *  Created on: Apr 3, 2014
 *      Author: Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/gfx/gfx.h>
#include <string.h>


int bitblt(bmp_t *dest, int dx, int dy, bmp_t *src, int sx, int sy, int w, int h, int rop)
{
//	int bpl, psw, padding;
	if (!dest || !src)
		return -EINVAL;

	if (dx > dest->width || dy > dest->height
			|| sx > src->width || sy > src->height)
		return -EINVAL;


	if (dx + w >= dest->width)
		w = dest->width-dx-1;
	if (dy + h >= dest->height)
		h = dest->height-dy-1;

	if (!w || !h)
		return -EINVAL;

//	padding = 0;
//	bpl = w * (src->bpp >> 3);
//	while ((bpl+padding) % 4 != 0)
//		padding++;
//	psw =bpl + padding;

	switch (dest->bpp) {
	case 8:
	case 16:
	case 24:
		switch (src->bpp) {
		case 8:
			/*
			 * should we convert src 8bpp to dest 24bpp?
			 */
			return -EINVAL;
		case 16:
			return -EINVAL;
		case 24:
			switch (rop) {
			case WHITENESS:
				__asm__(
						"cld\n\t"
						"1:"
						"push %%ecx\n\t"
						"movb $0xFF, %%al\n\t"
						"rep; stosb\n\t"
						"add %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl %%ebx\n\t"
						"testb %%bl, %%bl\n\t"
						"jnz 1b\n\t"
						:
						:"D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3),"b"(h),"d"((dest->width -w)*3));
				break;
			case BLACKNESS:
				__asm__(
						"cld\n\t"
						"1:"
						"push %%ecx\n\t"
						"movb $0x0, %%al\n\t"
						"rep; stosb\n\t"
						"add %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl %%ebx\n\t"
						"testb %%bl, %%bl\n\t"
						"jnz 1b\n\t"
						:
						:"D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3),"b"(h),"d"((dest->width -w)*3));
				break;
			case SRCCOPY:
				__asm__(
						"cld\n\t"
						"1:\n\t"
						"push %%ecx\n\t"
						"rep; movsb\n\t"
						"addl %%edx, %%edi\n\t"
						"addl %%eax, %%esi\n\t"
						"pop	%%ecx\n\t"
						"decl	%%ebx\n\t"
						"jnz	1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "d"((dest->width-w)*3),"a"((src->width-w)*3), "b"(h)
						);
				break;
			case MERGECOPY:
				return -ENOSYS;
			case SRCAND:
				__asm__(
						"1:\n\t"
						"pushl %%ecx\n\t"
						"pushl %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"andb  (%%edi), %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"popl %%eax\n\t"
						"popl %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3), "a"((src->width-w)*3));
				break;
			case SRCPAINT:
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"orb  (%%edi), %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"pop %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;
			case SRCINVERT:
				__asm__(
						"1:\n\t"
						"pushl %%ecx\n\t"
						"pushl %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"xorb (%%edi), %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"popl %%eax\n\t"
						"popl %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						//"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;
			case DSTINVERT:
				__asm__(
						"cld\n\t"
						"1:\n\t"
						"push %%ecx\n\t"
						"2:\n\t"
						"movb (%%edi), %%al\n\t"
						"xorb %%al, %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"addl %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl %%ebx\n\t"
						"testl %%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3),"b"(h),"d"((dest->width -w)*3));
				break;
			case MERGEPAINT:
				/*
				 * Merges the colors of the inverted source rectangle with
				 * the colors of the destination rectangle by using the Boolean OR operator.
				 */
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"xorb %%al, %%al\n\t"
						"orb (%%edi), %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"pop %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;
			case NOTSRCCOPY:
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"xorb %%al, %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"pop %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;

			case NOTSRCERASE:
				/*
				 * Combines the colors of the source and destination
				 * rectangles by using the Boolean OR operator and then inverts the resultant color.
				 */
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsb\n\t"
						"orb (%%edi), %%al\n\t"
						"xorb %%al, %%al\n\t"
						"stosb\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"pop %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;
			case PATCOPY:
			case PATINVERT:
			case PATPAINT:
				return -ENOSYS;
			case SRCERASE:
				/*
				 * Combines the inverted colors of the destination
				 * rectangle with the colors of the source rectangle by using the Boolean AND operator.
				 */
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"movb (%%edi), %%al\n\t"
						"xorb %%al, %%al\n\t"
						"andb (%%esi), %%al\n\t"
						"movb %%al, (%%edi)\n\t"
						"inc %%esi\n\t"
						"inc %%edi\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"pop %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w*3), "b"(h), "d"((dest->width-w)*3),"a"((src->width-w)*3));
				break;
			}
			break;
		}
		break;
	case 32:
		switch (src->bpp) {
		case 8:
		case 16:
			return -ENOSYS;
		case 24:	//24bit src to 32bit dest
			switch (rop) {
			case WHITENESS:
				__asm__(
						"cld\n\t"
						"1:"
						"push %%ecx\n\t"
						"movl $0xFFFFFFFF, %%eax\n\t"
						"rep; stosl\n\t"
						"add %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl %%ebx\n\t"
						"testl %%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w),"b"(h),"d"((dest->width -w)*(dest->bpp >> 3)));
				break;
			case BLACKNESS:
				__asm__(
						"cld\n\t"
						"1:"
						"push %%ecx\n\t"
						"movl $0x0, %%eax\n\t"
						"rep; stosl\n\t"
						"add %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl %%ebx\n\t"
						"testl %%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w),"b"(h),"d"((dest->width -w)*(dest->bpp >> 3)));
				break;
			case SRCCOPY:
				 asm volatile (
						"cld\n\t"
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsl\n\t"
						"andl $0x00FFFFFF, %%eax\n\t"
						"decl %%esi\n\t"
						"stosl\n\t"
						"loop 2b\n\t"
						"popl %%eax\n\t"
						"addl %%edx, %%edi\n\t"
						"addl %%eax, %%esi\n\t"
						"popl	%%ecx\n\t"
						"decl	%%ebx\n\t"
						"jnz	1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * 4),
						 "c"(w), "d"((dest->width-w)*4),"a"((src->width-w)*3), "b"(h)
						);
				break;
			case SRCPAINT:
				__asm__(
						"1:\n\t"
						"push %%ecx\n\t"
						"push %%eax\n\t"
						"2:\n\t"
						"lodsl\n\t"
						"andl $0x00FFFFFF, %%eax\n\t"
						"orl  (%%edi), %%eax\n\t"
						"decl %%esi\n\t"
						"stosl\n\t"
						"loop 2b\n\t"
						"pop %%eax\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"pop %%ecx\n\t"
						"decl	%%ebx\n\t"
						"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w), "b"(h), "d"((dest->width-w)*4),"a"((src->width-w)*3));
				break;
			case SRCINVERT:
				__asm__(
						"1:\n\t"
						"pushl %%ecx\n\t"
						"pushl %%eax\n\t"
						"2:\n\t"
						"lodsl\n\t"
						"xorl (%%edi), %%eax\n\t"
						"andl $0x00FFFFFF, %%eax\n\t"	//do not trigger alpha channel
						"decl %%esi\n\t"
						"stosl\n\t"
						"loop 2b\n\t"
						"popl %%eax\n\t"
						"popl %%ecx\n\t"
						"addl %%eax, %%esi\n\t"
						"addl %%edx, %%edi\n\t"
						"decl	%%ebx\n\t"
						//"testl	%%ebx, %%ebx\n\t"
						"jnz 1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * (src->bpp >> 3)),
						 "D"(dest->data + (dy*dest->width + dx) * (dest->bpp >> 3)),
						 "c"(w), "b"(h), "d"((dest->width-w)*4),"a"((src->width-w)*3));
				break;

			}
			break;
		case 32: //32bit src -> 32bit dest
			switch (rop) {
			case SRCCOPY:
				 asm volatile (
						"cld\n\t"
						"1:\n\t"
						"push %%ecx\n\t"
						"rep; movsl\n\t"
						"addl %%edx, %%edi\n\t"
						"addl %%eax, %%esi\n\t"
						"popl	%%ecx\n\t"
						"decl	%%ebx\n\t"
						"jnz	1b\n\t"
						:
						:"S"(src->data + (sy*src->width + sx) * 4),
						 "D"(dest->data + (dy*dest->width + dx) * 4),
						 "c"(w), "d"((dest->width-w)*4),"a"((src->width-w)*4), "b"(h)
						);
			}
			break;


		}
		break;

	}

	return 0;

}
