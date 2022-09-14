/*
 * stdarg.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_STDARG_H_
#define SHAUN_STDARG_H_


#define STDARG


typedef char* va_list;


#ifndef STDARG
#define __va_size(type) \
   (((sizeof(type) + sizeof(long) - 1) / sizeof (long)) * sizeof (long))

#define va_start(ap, last) \
   ((ap) = ((char*)&last) + __va_size(last))

#define va_arg(ap, type) \
   (*(type*)((ap)+= __va_size(type), (ap) - __va_size(type)))

#define va_end(va_list) ((void)0)
#else
#define va_start(v,l)	__builtin_va_start(v,l)
#define va_end(v)	__builtin_va_end(v)
#define va_arg(v,l)	__builtin_va_arg(v,l)
#endif


#endif /* SHAUN_STDARG_H_ */
