/*
 * avltree.c
 *
 *  Created on: Jan 14, 2013
 *      Author: Shaun Yuan
 *
 *      basic avl tree implementation,
 *      it is really funny and
 *      amazing
 */


#include <kernel/avltree.h>
#include <kernel/assert.h>
#include <kernel/malloc.h>
#include <kernel/types.h>
#include <kernel/kernel.h>

static int inline Max(int a, int b)
{
	return (a) > (b) ? (a) : (b);
}

static int height(struct avlnode *p)
{
	if (p == NULL)
		return -1;
	else
		return p->height;
}

/*
 * for left-left
 */
static struct avlnode *
single_rotate_with_left(struct avlnode *p)
{
	struct avlnode *p1;
	p1 = p->left;
	p->left = p1->right;
	p1->right = p;

	p->height = Max(height(p->left), height(p->right)) + 1;
	p1->height = Max(height(p1->left), height(p1->right)) + 1;

	return p1;
}
/*
 * for right-right
 */
static struct avlnode *
single_rotate_with_right(struct avlnode *p)
{
	struct avlnode *p1;
	p1 = p->right;
	p->right = p1->left;
	p1->left = p;

	p->height = Max(height(p->left), height(p->right)) + 1;
	p1->height = Max(height(p1->left), height(p1->right)) + 1;

	return p1;
}

/*
 * for left-right
 */
static struct avlnode *
double_rotate_with_left(struct avlnode *p)
{
	p->left = single_rotate_with_right(p->left);
	return single_rotate_with_left(p);
}

/*
 * for right-left
 */
static struct avlnode *
double_rotate_with_right(struct avlnode *p)
{
	p->right = single_rotate_with_left(p->right);
	return single_rotate_with_right(p);
}

struct avlnode *
avl_insert(unsigned long val,void *payload, struct avlnode *p)
{
	if (p == NULL){
		p = kmalloc(sizeof(struct avlnode));
		assert(p != NULL);
		p->val = val;
		p->height = 0;
		p->left = p->right = NULL;
		p->payload = payload;
	}
	else if (val < p->val){
		p->left = avl_insert(val, payload, p->left);
		if ((height(p->left) - height(p->right)) == 2){
			if (val < p->left->val)
				p = single_rotate_with_left(p);
			else
				p = double_rotate_with_left(p);
		}
	}
	else if (val > p->val){
		p->right = avl_insert(val, payload, p->right);
		if (height(p->right) - height(p->left) == 2) {
			if (val > p->right->val)
				p = single_rotate_with_right(p);
			else
				p = double_rotate_with_right(p);
		}
	}

	p->height = Max(height(p->left), height(p->right)) + 1;
	return p;
}

struct avlnode *
avl_delete(unsigned long val,struct avlnode *p)
{
	struct avlnode *tmp;
	if (p == NULL){
		return NULL;
	}
	else if (val < p->val){
		p->left = avl_delete(val, p->left);
		if (height(p->right) - height(p->left) == 2){
			tmp = p->right;
			if (height(tmp->right) > height(tmp->left))
				p = double_rotate_with_right(p);
			else
				p = single_rotate_with_right(p);
		} else
			p->height = Max(height(p->left), height(p->right)) + 1;
	}
	else if (val > p->val){
		p->right = avl_delete(val, p->right);
		if (height(p->left) - height(p->right) == 2){
			tmp = p->left;
			if (height(tmp->left) > height(tmp->right))
				p = single_rotate_with_left(p);
			else
				p = double_rotate_with_left(p);
		}
		else
			p->height = Max(height(p->left), height(p->right)) + 1;
	}
	else if (p->left && p->right){
		tmp = p->right;
		while (tmp->left != NULL)
			tmp = tmp->left;

		p->val = tmp->val;
		p->payload = tmp->payload;
		p->right = avl_delete(tmp->val, p->right);
	}
	else {
		tmp = p;
		if (p->left == NULL)
			p = p->right;
		else if (p->right == NULL)
			p = p->left;

		kfree(tmp);
	}

	return p;
}

void
avl_destroy_tree(struct avlnode *root)
{
	if (root){
		avl_destroy_tree(root->right);
		avl_destroy_tree(root->left);
		kfree(root);
		root = NULL;
	}
}
struct avlnode *
avl_delete_node(u32 val, struct avlnode *root)
{
	return avl_delete(val, root);
}

struct avlnode *
avl_find(unsigned long val, struct avlnode *p)
{
	if (p == NULL)
		return NULL;
	if (val > p->val)
		return avl_find(val, p->right);
	else if (val < p->val)
		return avl_find(val, p->left);
	else
		return p;

}


void OUT(struct avlnode *T)
{  
    if(T->left){
        printk("Left\t%d\t[parent:%d]\n", T->left->val, T->val);
        OUT(T->left);
    }
    if(T->right){
        printk("Right\t%d\t[parent:%d]\n", T->right->val, T->val);
        OUT(T->right);
    }
}


