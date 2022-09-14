/*
 * vsprintf.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */
#include <stdarg.h>
#include <kernel/types.h>
#include <string.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>

//static  char buf[1024];

extern void write_string( const char *s , unsigned long len);

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

#define SIGN 	(1<<0)
#define LEFT	(1<<1)
#define SPACE	(1<<2)
#define ZEROPAD	(1<<3)
#define SPECIAL	(1<<4)
#define SMALL	(1<<5)
#define PLUS	(1<<6)



static char *number(char *buf,  unsigned long long num,
			u32 flags, int base, int width, int precision)
{
	/* we are called with base 8, 10 or 16, only, thus don't need "G..."  */
	static const char digits[16] = "0123456789ABCDEF"; /* "GHIJKLMNOPQRSTUVWXYZ"; */

	char tmp[66];
	char sign;
	char locase;
	int need_pfx = ((flags & SPECIAL) && base != 10);
	int i;

	/* locase = 0 or 0x20. ORing digits or letters with 'locase'
	 * produces same digits or (maybe lowercased) letters */
	locase = (flags & SMALL);
	if (flags & LEFT)
		flags &= ~ZEROPAD;
	sign = 0;
	if (flags & SIGN) {
		if ((signed long long)num < 0) {
			sign = '-';
			num = -(signed long long)num;
			width--;
		} else if (flags & PLUS) {
			sign = '+';
			width--;
		} else if (flags & SPACE) {
			sign = ' ';
			width--;
		}
	}
	if (need_pfx) {
		width--;
		if (base == 16)
			width--;
	}

	/* generate full string in tmp[], in reverse order */
	i = 0;
	if (num == 0)
		tmp[i++] = '0';
	/* Generic code, for any base:
	else do {
		tmp[i++] = (digits[do_div(num,base)] | locase);
	} while (num != 0);
	*/
	else if (base != 10) { /* 8 or 16 */
		int mask = base - 1;
		int shift = 3;

		if (base == 16)
			shift = 4;
		do {
			tmp[i++] = (digits[((unsigned char)num) & mask] | locase);
			num >>= shift;
		} while (num);
	} else { /* base 10 */
		//i = put_dec(tmp, num) - tmp;
		do {
			tmp[i++] = (digits[do_div(num,base)] | locase);
		} while (num != 0);

	}

	/* printing 100 using %2d gives "100", not "00" */
	if (i > precision)
		precision = i;
	/* leading space padding */
	width -= precision;
	if (!(flags & (ZEROPAD+LEFT))) {
		while (--width >= 0) {
				*buf = ' ';
			++buf;
		}
	}
	/* sign */
	if (sign) {
			*buf = sign;
		++buf;
	}
	/* "0x" / "0" prefix */
	if (need_pfx) {
			*buf = '0';
		++buf;
		if (base == 16) {
				*buf = ('X' | locase);
			++buf;
		}
	}
	/* zero or space padding */
	if (!(flags & LEFT)) {
		char c = (flags & ZEROPAD) ? '0' : ' ';
		while (--width >= 0) {
				*buf = c;
			++buf;
		}
	}
	/* hmm even more zero padding? */
	while (i <= --precision) {
			*buf = '0';
		++buf;
	}
	/* actual digits of result */
	while (--i >= 0) {
			*buf = tmp[i];
		++buf;
	}
	/* trailing space padding */
	while (width >= 0) {
			*buf = ' ';
		++buf;
	}

	return buf;
}


int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	size_t len = size;
	long long num;
	u32 flag = 0;
	int width = 0, precesion = 0, dot = 0, base = 10;
	char *p = str;
	char ch;
	char qualifier;
	while (*fmt && len){
		if (*fmt != '%'){
			*str++ = *fmt++;
			len--;
			continue;
		}
		flag = 0;
		qualifier = 0;
		base=10;
		dot=0;
		precesion=0;
		fmt++;	//skip %
reswitch:
		switch ( ch = *fmt++){
		case '#':
			flag |= SPECIAL;
			goto reswitch;
		case '+':
			flag |= PLUS;
			goto reswitch;
		case '-':
			flag |= LEFT;
			goto reswitch;
		case ' ':
			flag |= SPACE;
			goto reswitch;
		case '0':
			flag |= ZEROPAD;
			goto reswitch;
		case '%':
			*str++ = '%';
			len--;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':{
			int n;
			for (n=0; ; ++fmt){
				n = n * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			if (dot)
				precesion = n;
			else
				width = n;
			goto reswitch;
		}
		case '*':
			if (dot)
				precesion = va_arg(ap, int);
			else
				width = va_arg(ap, int);
			len--;
			break;
		case '.':
			dot = 1;
			goto reswitch;
		case 'c':{
			char c;
			if (!(flag & LEFT)){
				while (--width > 0)
					*str++ = ' ';
			}
			c = (u8)va_arg(ap, int);
			*str++ = c;
			len--;
			while (--width > 0)
				*str++ = ' ';
			}
			break;
		case 's':{
			char *s = va_arg(ap, char *);
			if (s == NULL)
				s = "(null)";
			int n;
			if (!dot)
				n = strlen(s);
			else
				for (n=0; n<precesion && s[n]; n++);

			width -= n;
			if (!(flag & LEFT) && width > 0){
				while (width-- > 0)
					*str++ = ' ';
			}
			while (n-- > 0){
				*str++ = *s++;
				len--;
			}

			if ((flag & LEFT) && width > 0){
				while (width-- > 0)
					*str++ = ' ';
			}

		}
			break;
		case 'i':
		case 'd':
			flag |= SIGN;
			base = 10;
			goto handle_sign;
		case 'L':
			qualifier = 'L';
			goto reswitch;
		case 'H':
			qualifier = 'H';
			goto reswitch;
		case 'l':
			if (qualifier == 'l')
				qualifier = 'L';
			else
				qualifier = 'l';
			goto reswitch;
		case 'z':
			qualifier = 'z';
			goto reswitch;
		case 'Z':
			qualifier = 'z';
			goto reswitch;
		case 'h':
			if (qualifier == 'h')
				qualifier = 'H';
			else
				qualifier = 'h';
			goto reswitch;
		case 't':
			qualifier = 't';
			goto reswitch;
		case 'p':
			base = 16;
			num = (unsigned int)va_arg(ap, void *);
			goto handle_number;
		case 'o':
			base = 8;
			goto handle_nosign;
		case 'u':
			base = 10;
			goto handle_nosign;
		case 'x':
			flag |= SMALL;
			base = 16;
			goto handle_nosign;
		case 'X':
			base = 16;
			goto handle_nosign;
handle_nosign:
			switch(qualifier){
			case 'L':
				num = (long long)va_arg(ap, long long);
				goto handle_number;
			case 'l':
				num = va_arg(ap, unsigned long);
				goto handle_number;
			case 'h':
				num = (u16)va_arg(ap, int);
				goto handle_number;
			case 'H':
				num = (u8)va_arg(ap, int);
				goto handle_number;
			case 'z':
				num = va_arg(ap, size_t);
				goto handle_number;
			case 't':
				num = va_arg(ap, long);
				goto handle_number;
			default:
				num = va_arg(ap, unsigned int);
				goto handle_number;
			}
handle_sign:
			switch(qualifier){
			case 'L':
				num = (long long)va_arg(ap, long long);
				goto handle_number;
			case 'l':
				num = va_arg(ap, long);
				goto handle_number;
			case 'h':
				num = (short)va_arg(ap, int);
				goto handle_number;
			case 'H':
				num = (char)va_arg(ap, int);
				goto handle_number;
			case 'z':
				num = va_arg(ap, ssize_t);
				goto handle_number;
			case 't':
				num = va_arg(ap, long);
				goto handle_number;
			default:
				num = va_arg(ap, int);
				goto handle_number;
			}
handle_number:
		str = number(str, num, flag, base, width, precesion);
		}
	}
	*str = '\0';
	return str - p;
}

int snprintf (char *str, size_t size, const  char  *format,...)
{
	int i;
	va_list args;
	va_start(args,format);
	i = vsnprintf(str, size, format, args);
	va_end(args);
	return i;
}

int sprintf (char *str, const  char  *format,...)
{
	int i;
	va_list args;
	va_start(args,format);
	i = vsnprintf(str, 0xFFFFFFFFUL, format, args);
	va_end(args);

	return i;

}

int printk( const char *format, ...)
{
	int i;
	va_list	args;
	char *buf = kzalloc(256);
	assert( buf != NULL);
	va_start(args, format);
	//memset(buf, 0, 1024);
	i = vsnprintf(buf, 0xFFFFFFFFUL, format, args);
	va_end(args);
	write_string(buf, i);
	kfree(buf);
	return i;
}

