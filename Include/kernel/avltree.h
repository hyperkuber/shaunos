/*
 * avltree.h
 *
 *  Created on: Jan 14, 2013
 *      Author: root
 */

#ifndef AVLTREE_H_
#define AVLTREE_H_

struct avlnode {
	unsigned long val;
	struct avlnode *left;
	struct avlnode *right;
	int height;
	void *payload;
};
typedef struct avlnode avltree_t;

struct avlnode *
avl_insert(unsigned long val,void *payload, struct avlnode *p);

struct avlnode *
avl_delete(unsigned long val,struct avlnode *p);

struct avlnode *
avl_find(unsigned long val, struct avlnode *p);

void
avl_destroy_tree(struct avlnode *root);

struct avlnode *
avl_delete_node(unsigned long val, struct avlnode *root);

void OUT(struct avlnode *T);

#endif /* AVLTREE_H_ */
