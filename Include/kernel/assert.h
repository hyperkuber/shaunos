/*
 * assert.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef ASSERT_H_
#define ASSERT_H_
#include <kernel/kernel.h>

void static __assert_fail (__const char *__assertion, __const char *__file,
			   unsigned int __line, __const char *__function)
{
	printk("Assertion failed:\n%s in\nFile:%s Line:%d Function:%s\n",
			__assertion, __file, __line, __function);
	BUG();

}

#define __ASSERT_VOID_CAST	(void)
#define __STRING(x)	#x
//gcc defines __PRETTY_FUNCTION__, a kind of __func__ in c9x,
// and it works well for g++
#define __ASSERT_FUNCTION	__PRETTY_FUNCTION__

#define assert(expr)							\
  do{ ((expr) ? __ASSERT_VOID_CAST (0)			\
		  : __assert_fail (__STRING(expr), __FILE__, __LINE__, __ASSERT_FUNCTION));	\
  } while (0)

#endif /* ASSERT_H_ */
