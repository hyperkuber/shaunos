/*
 * string.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <string.h>
#include <kernel/malloc.h>
#include <kernel/kthread.h>


int strcmp(const char *s1, const char *s2)
{
	if (s1 == NULL || s2 == NULL)
		return -1;

	char *p1 = (char *)s1;
	char *p2 = (char *)s2;

	while (*p1 && *p2 && (*p1 == *p2)){
		p1++;
		p2++;
	}
	return (*p1 - *p2);
}


int strncmp(const char *s1, const char *s2, ssize_t n)
{
	if (s1 == NULL || s2 == NULL)
		return -1;

	char *p1 = (char *)s1;
	char *p2 = (char *)s2;

	while (n-- && *p1 && *p2 && (*p1 == *p2)){
		p1++;
		p2++;
	}

	return (*p1 - *p2);
}

char *strcat(char *dest, const char *src)
{
	if (dest == NULL || src == NULL)
		return dest;
	char *d = dest;
	char *s = (char *)src;

	asm volatile (
			"cld\n\t"
			"1:scasb\n\t"
			"jnz 1b\n\t"
			"decl %1\n\t"
			"2:lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jnz 2b\n\t"
			:"=S"(s), "=D"(d)
			:"0"(s), "1"(d), "a"(0)
			:"memory");

	return d;
}

char *strncat(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL || n == 0)
		return dest;

	char *d = dest;
	char *s = (char *)src;

	asm volatile (
			"cld\n\t"
			"1:scasb\n\t"
			"jnz 1b\n\t"
			"decl %1\n\t"
			"2:decl %2\n\t"
			"js 3f\n\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jnz 2b\n\t"
			"3:xorl %5, %5\n\t"
			"stosb\n\t"
			:"=S"(s), "=D"(d), "=c"(n)
			:"0"(s), "1"(d), "a"(0), "2"(n)
			:"memory");

	return dest;
}




/*
 * length does not contain the NULL
 * character
 */
size_t strlen(const char *s)
{
	if (s == NULL)
		return -1;

	char *p = (char *)s;
	size_t len = 0;

	asm volatile (
			"cld\n\t"
			"1:scasb\n\t"
			"jnz 1b\n\t"
			:"=D"(len)
			:"a"(0),"0"(p)
			:"memory");

	return (len - (unsigned long)p - 1);
}

size_t strnlen(const char *s, int maxlen)
{
	size_t n = strlen(s);

	if (n<=maxlen)
		return n;
	else
		return maxlen;
}


int
memcmp(const void *s1, const void *s2, size_t n)
{
	if (n != 0) {
		const unsigned char *p1 = s1, *p2 = s2;

		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--n != 0);
	}
	return (0);
}



void *memset(void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s;
	size_t i;
	int uc = c * 0x01010101UL;
	if (n == 0)
		return s;

	if (n < 32){
		for (i=0; i<n; i++)
			*p++ = (unsigned char)c;
		return s;
	}
	n -= (-(unsigned long)p % 4);
	//the address alignment check is needed
	switch((unsigned long)p & 3){
	case 1:
		*p++ = (unsigned char)c;
	case 2:
		*p++ = (unsigned char)c;
	case 3:
		*p++ = (unsigned char)c;
	case 0:	//here, p must align by 4
		do {
			switch(n & 3){
			case 0:
				do {
					asm volatile (
							"cld\n\t"
							"rep; stosl\n\t"
							:
							:"a"(uc), "c"(n/4), "D"(p)
							:"memory");

				} while (0);
				break;
			case 1:
				do {
					asm volatile (
							"cld\n\t"
							"rep; stosl\n\t"
							"stosb\n\t"
							:
							:"a"(uc), "c"(n/4), "D"(p)
							:"memory");
				} while (0);
				break;
			case 2:
				do {
					asm volatile (
							"cld\n\t"
							"rep; stosl\n\t"
							"stosw\n\t"
							:
							:"a"(uc), "c"(n/4), "D"(p)
							:"memory");

				}while (0);
				break;
			case 3:
				do {
					asm volatile (
							"cld\n\t"
							"rep; stosl\n\t"
							"stosw\n\t"
							"stosb\n\t"
							:
							:"a"(uc), "c"(n/4), "D"(p)
							:"memory");
				}while (0);
			}

		} while (0);
		break;
	}

	return s;
}


/*
 * memcpy do not care about memory overlap
 */
void *memcpy(void *dest, const void *src, size_t n)
{

	if (src == NULL || dest == NULL)
		return NULL;

	unsigned char *d = (unsigned char *)dest;
	unsigned char *s = (unsigned char *)src;

	if (n == 0)
		return dest;

	if (n < 32){
		asm volatile (
				"cld\n\t"
				"rep; movsb\n\t"
				:
				:"D"(d), "S"(s), "c"(n)
				:"memory");
		return dest;
	}

	n -= (-(unsigned long)dest % 4);
	//ignore the alignment of src,
	//just take care of dest
	switch((unsigned long)dest & 3){
	case 1:
		*d++ = *s++;
	case 2:
		*d++ = *s++;
	case 3:
		*d++ = *s++;
	case 0:
		do {
			switch (n & 3){
			case 0:
				do {
					asm volatile (
							"cld\n\t"
							"rep; movsl\n\t"
							:"=D"(d), "=S"(s), "=c"(n)
							:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
							:"memory");
				} while (0);
				break;
			case 1:
				do {
					asm volatile (
							"cld\n\t"
							"rep; movsl\n\t"
							"movsb\n\t"
							:"=D"(d), "=S"(s), "=c"(n)
							:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
							:"memory");
				} while (0);
				break;
			case 2:
				do {
					asm volatile (
							"cld\n\t"
							"rep; movsl\n\t"
							"movsw\n\t"
							:"=D"(d), "=S"(s), "=c"(n)
							:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
							:"memory");
				} while (0);
				break;
			case 3:
				do {
					asm volatile (
							"cld\n\t"
							"rep; movsl\n\t"
							"movsw\n\t"
							"movsb\n\t"
							:"=D"(d), "=S"(s), "=c"(n)
							:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
							:"memory");
				} while (0);
				break;
			}
		} while (0);
		break;
	}

	return dest;
}

char *strcpy(char *dest, const char *src)
{
	if (dest == NULL || src == NULL)
		return NULL;

	char *p = (char *)src;

	//while((*dest++ = *p++) != '\0');

	asm volatile (
			"cld\n\t"
			"1: lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jnz 1b\n\t"
			:
			:"S"((unsigned long)p), "D"((unsigned long)dest)
			:"memory");

	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	char *s = (char *)src;
	char *d = dest;

	if (n == 0)
		return dest;

//	if (*s == 0)
//		return dest;
	//bug fix:If there is no null byte among the first n bytes
	//of src, the string placed in dest will __be__ null terminated.
	//the standard contains no null character
	asm volatile (
			"cld\n\t"
			"1:decl %2\n\t"
			"js 2f\n\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jnz 1b\n\t"
			"rep; stosb\n\t"
			"jmp 3f\n\t"
			"2:xor %%eax, %%eax\n\t"
			"stosb\n\t"
			"3:"
			:"=D"(d), "=S"(s), "=c"(n)
			:"1"(s), "0"(d), "2"(n)
			:"memory");

	return dest;
}

char *strchr(const char *s, int c)
{
	char *p = (char *)s;
	asm volatile (
			"cld\n\t"
			"1:lodsb\n\t"
			"cmpb %%al, %%bl\n"
			"je 2f\n\t"
			"testb %%al, %%al\n\t"
			"jnz 1b\n\t"
			"movl $0, %0\n\t"
			"jmp 3f\n\t"
			"2:decl %0\n\t"
			"3:"
			:"=S"(p)
			:"b"(c), "0"(p));

	return p;
}

char *strrchr(const char *s, int c)
{
	size_t len = strlen(s);
	char *p = (char *)s + len;

	asm volatile (
			"std\n\t"
			"1:lodsb\n\t"
			"cmpb %%al, %%bl\n\t"
			"je 2f\n\t"
			"decl %2\n\t"
			"jnz 1b\n\t"
			"movl $0, %0\n\t"
			"jmp 3f\n\t"
			"2: incl %0\n\t"
			"3:"
			:"=S"(p)
			:"0"(p), "c"(len), "b"(c)
			:);

	return  p;
}


char *strdup(const char *s)
{
	int len = strlen(s) + 1;
	char *ret = (char *)kmalloc(len);
	if (ret == NULL)
		return NULL;

	memcpy((void *)ret, (void *)s, len);
	return ret;
}


void *memmove(void *dest, const void *src, size_t n)
{
	if (dest == NULL || src == NULL || n == 0)
		return dest;

	char *s = (char *)src;
	char *d = (char *)dest;

	if (dest > src && (unsigned long)dest <= ((unsigned long)src + n)){
		//overlap
		s += n;
		d += n;
		n -= (-(unsigned long)d % 4);
		switch((unsigned long)d & 3){
		case 1:
			*d-- = *s--;
		case 2:
			*d-- = *s--;
		case 3:
			*d-- = *s--;
		case 0:
			do {
				switch (n & 3){
				case 0:
					do {
						asm volatile (
								"std\n\t"
								"rep; movsl\n\t"
								:"=D"(d), "=S"(s), "=c"(n)
								:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
								:"memory");
					} while (0);
					break;
				case 1:
					do {
						asm volatile (
								"std\n\t"
								"rep; movsl\n\t"
								"movsb\n\t"
								:"=D"(d), "=S"(s), "=c"(n)
								:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
								:"memory");
					} while (0);
					break;
				case 2:
					do {
						asm volatile (
								"std\n\t"
								"rep; movsl\n\t"
								"movsw\n\t"
								:"=D"(d), "=S"(s), "=c"(n)
								:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
								:"memory");
					} while (0);
					break;
				case 3:
					do {
						asm volatile (
								"std\n\t"
								"rep; movsl\n\t"
								"movsw\n\t"
								"movsb\n\t"
								:"=D"(d), "=S"(s), "=c"(n)
								:"0"((unsigned long)d), "1"((unsigned long)s), "2"(n/4)
								:"memory");
					} while (0);
					break;
				}
			} while (0);
			break;
		}
	} else
		return memcpy(dest, src, n);

	return dest;
}



int strnlen_user(const char *path, int size)
{
	int retval;

	retval = probe_for_read((unsigned long)path, size);
	if (retval < 0)
		return retval;

	return strnlen(path, size);

}

int strlen_user(char *path)
{
	int ret;
	ret = strlen(path);

	if (probe_for_read((u32)path, ret) < 0)
		return -EFAULT;
	return ret;
}









