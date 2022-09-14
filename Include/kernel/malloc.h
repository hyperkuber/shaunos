/*
 * malloc.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef MALLOC_H_
#define MALLOC_H_



void heap_init(unsigned long start, unsigned long size);
void *kmalloc(unsigned long size);
void kfree(void *buf);
void *kzalloc(unsigned long size);

#endif /* MALLOC_H_ */
