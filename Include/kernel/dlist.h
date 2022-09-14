/*
 * dlist.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef SHAUN_DLIST_H_
#define SHAUN_DLIST_H_


#define offsetof(TYPE, MEMBER) ((unsigned long) &(((type*)0)->member))

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


struct dlist {
	struct dlist *next, *prev;
};

static inline void dlist_init(struct dlist *list)
{
	list->next = list;
	list->prev = list;
}

/*
 * add new item to the list
 * @new: new entry to be inserted
 * @head: list head
 *
 * Description: insert after the head
 * used for queue implementation
 *
 */

static inline void dlist_add_tail(struct dlist *entry, struct dlist *head)
{
	head->prev->next = entry;
	entry->next = head;
	entry->prev = head->prev;
	head->prev = entry;
}


static inline void dlist_add(struct dlist *entry, struct dlist *head)
{
	head->next->prev = entry;
	entry->next = head->next;
	entry->prev = head;
	head->next = entry;
}

/*
 * delete the entry from his list
 * @entry : to be deleted
 */
static inline void dlist_del(struct dlist *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;

	entry->next = NULL;
	entry->prev = NULL;

}

static inline void dlist_move_tail(struct dlist *list,
				  struct dlist *head)
{
	list->prev->next = list->next;
	list->next->prev = list->prev;

	head->prev->next = list;
	list->next = head;
	list->prev = head->prev;
	head->prev = list;

}

static inline int dlist_is_last(const struct dlist *list,
				const struct dlist *head)
{
	return list->next == head;
}

static inline int dlist_empty(const struct dlist *head)
{
	return head->next == head;
}

/*
*  join two lists
*  @list: new list's head to be joined
*  @head: the head to place the new list
*  see note before using
*/

static inline void dlist_splice(struct dlist *list, struct dlist *head)
{
	struct dlist *first;
	struct dlist *last;
	if (!dlist_empty(list))
		{
			//note: here we cut the head node, so be the @list is the head node
			//if @list is first entry, you will lost one item,
			first = list->next;
			last = list->prev;

			first->prev = head;
			head->next->prev = last;
			last->next = head->next;
			head->next = first;

		}
}

static inline void dlist_splice_tail(struct dlist *list, struct dlist *head)
{
	struct dlist *first;
	struct dlist *last;

	if (!dlist_empty(list))
		{
			first = list->next;
			last = list->prev;


			head->prev->next = first;
			first->prev = head->prev;
			last->next = head;
			head->prev = last;
		}
}


#endif /* SHAUN_DLIST_H_ */
