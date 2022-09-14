/*
 * list.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef LIST_H_
#define LIST_H_

#include <kernel/types.h>

struct linked_list {
	struct linked_list *prev;
	struct linked_list *next;
};

#undef offsetof

#define offsetof(TYPE, MEMBER) ((unsigned long) &(((TYPE*)0)->MEMBER))

#define container_of(ptr, type, member)	({ \
		const typeof (((type *)0)->member) * __mptr = (ptr);	\
		(type *)((char *) __mptr - offsetof(type,member));})






static void inline list_init(struct linked_list *head)
{
	if (head != NULL){
		head->prev = head;
		head->next = head;
	}
}

static int inline list_is_empty(struct linked_list *head)
{
	return head->next == head;
}

static void inline list_add(struct linked_list *head, struct linked_list *entry)
{
	entry->next = head->next;
	head->next->prev = entry;
	head->next = entry;
	entry->prev = head;

}

static void inline list_add_tail(struct linked_list *head, struct linked_list *entry)
{
	//here, we can get the last element of the head, and call
	//list_add (last, entry), i choose another way, just for
	//Performance, though it is a inline function, do not
	//trust gcc absolutely
	head->prev->next = entry;
	entry->next = head;
	entry->prev = head->prev;
	head->prev = entry;
}


static void inline list_del( struct linked_list *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;

	entry->next = entry;
	entry->prev = entry;
}

static void inline list_move_head(struct linked_list *head, struct linked_list *entry)
{
	list_del(entry);
	list_add(head, entry);
}


static void inline list_move_tail(struct linked_list *head, struct linked_list *entry)
{

	list_del(entry);
	list_add_tail(head, entry);
}

/*
 * subtract a list from list head,
 * sub list is from head->next to where->prev
 * and sub list is still a linked list
 *
 * return the sub list head
 */
static inline struct linked_list *
list_cut(struct linked_list *head, struct linked_list *where)
{
	struct linked_list *sub_head;
	if (list_is_empty(head))
		return NULL;

	sub_head = head->next;

	if (sub_head == where){
		head->prev->next = sub_head;
		sub_head->prev = head->prev;
		head->next = head;
		head->prev = head;
		return sub_head;
	}


	head->next = where;
	sub_head->prev = where->prev;
	where->prev->next = sub_head;
	where->prev = head;

	return sub_head;
}

/*
 * join two lists,
 * sub_list is close to head
 */
static void inline
list_splice(struct linked_list *head, struct linked_list *sub_list)
{
	struct linked_list *last;
	last = sub_list->prev;

	last->next = head->next;
	head->next->prev = last;
	head->next = sub_list;
	sub_list->prev = head;
}

/*
 * join two lists,
 * sub_list is at the end of head list
 */
static void inline
list_splice_tail(struct linked_list *head, struct linked_list *sub_list)
{

	struct linked_list *last;
	last = sub_list->prev;
	head->prev->next = sub_list;
	sub_list->prev = head->prev;
	last->next = head;
	head->prev = last;

}





#endif /* LIST_H_ */


