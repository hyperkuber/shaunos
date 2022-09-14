/*
 * block_dev.c
 *
 * Copyright (c) 2012 Shaun Yuan
 * 
 */

#include <kernel/io_dev.h>
#include <list.h>
#include <kernel/malloc.h>
#include <string.h>
#include <kernel/assert.h>
#include <asm/io.h>
#include <kernel/errno.h>
#include <kernel/kthread.h>



kthread_u_t *io_scheduler;
static struct linked_list dev_list_head = {
		&dev_list_head,
		&dev_list_head,
};

struct linked_list io_request_list_head = {
		&io_request_list_head,
		&io_request_list_head,
};

int register_device(const char *name, dev_ops_t *ops, int devid, void *data)
{
	dev_t *dev = NULL;
	dev = (dev_t *)kmalloc(sizeof(dev_t));
	if (dev == NULL){
		return -1;
	}

	dev->name = (char *)name;
	dev->ops = ops;
	dev->devid = devid;
	dev->driver_data = data;
	dev->inuse = 0;

	list_init(&dev->dev_list);
	list_add_tail(&dev_list_head, &dev->dev_list);

	return 0;
}

int open_device(const char *devname, dev_t **pdev)
{
	dev_t	*dev = NULL;
	struct linked_list *pos = dev_list_head.next;
	int ret;
	assert(pos != NULL);

	for (; pos != &dev_list_head; pos = pos->next){
		dev = container_of(pos, dev_t, dev_list);
		assert(dev != NULL);
		if (strcmp(dev->name, devname) == 0){
			ret = dev->ops->open(dev);
			if (ret == 0){
				*pdev = dev;
				dev->inuse++;
				return 0;
			}
		}
	}

	return -ENODEV;
}

int close_device(dev_t *dev)
{
	int ret;
	ret = dev->ops->close(dev);
	return 0;
}

io_req_t *io_create_req_blk(dev_t *dev, enum request_type type,
    u32 block_start, u32 block_nums, void *buf)
{
	io_req_t *req = (io_req_t *)kmalloc(sizeof(io_req_t));
    if (req != 0) {
    	memset((void *)req, 0, sizeof(*req));
    	req->dev = dev;
    	req->flag |= IO_TARGET_BLOCK;
    	req->io_req_type = type;
    	req->_u.b_req.block_start= block_start;
    	req->_u.b_req.block_nums = block_nums;
    	req->buf = buf;
    	req->io_state = IO_PENDING;
    	req->error = 0;
    	list_init(&req->list);
    	list_init(&req->waiter.waiter);
    	return req;
    }
    return NULL;
}


io_req_t *
io_create_req_chr(dev_t *dev, enum request_type type, u32 nr_bytes, void *buf)
{
	io_req_t *req = (io_req_t *)kmalloc(sizeof (io_req_t));
	if (req != NULL){
		memset((void *)req, 0, sizeof(*req));
		req->dev = dev;
		req->flag |= IO_TARGET_CHAR;
		req->io_req_type = type;
		req->_u.c_req.nr_bytes = nr_bytes;
		req->buf = buf;
		req->io_state = IO_PENDING;
    	list_init(&req->list);
    	list_init(&req->waiter.waiter);
		return req;
	}

	return NULL;

}



void io_free_req(io_req_t *req)
{
	kfree((void *)req);
}


void io_wait(wait_queue_head_t *wq)
{
	if (wq){
		int x;
		local_irq_save(&x);
		list_move_tail(&wq->waiter, &current->thread_list);
		local_irq_restore(x);
		schedule();
	}
}

void io_wakeup(wait_queue_head_t *wq)
{
	wakeup_waiters(wq);
}

void io_post_request(io_req_t *req)
{
	if (req) {
	    list_add_tail(&io_request_list_head, &req->list);
	    pt_wakeup(&io_scheduler->kthread);
	    io_wait(&req->waiter);
	}


}

void io_send_request(io_req_t *req)
{
	int x;
	for(;;) {
		pt_wakeup(&io_scheduler->kthread);
		local_irq_save(&x);
		list_add_tail(&io_request_list_head, &req->list);
		io_wait(&req->waiter);
		local_irq_restore(x);

		if (req->io_state == IO_PENDING){
			continue;
		}
		break;
	}

	return;
}


void* io_schedule(void * args)
{
	struct linked_list *pos = NULL;
	while (1){
		pos = io_request_list_head.next;
		if (pos == &io_request_list_head){
			schedule();
			continue;
		}
		io_req_t *req = container_of(pos, io_req_t, list);
		list_del(&req->list);
		req->dev->ops->io_request(req);
		wakeup_waiters(&req->waiter);
	};

	return NULL;
}

int io_request(io_req_t *req)
{

    if (req == NULL)
    	return -EINVAL;

    if (req->flag & IO_AYSNC){
    	io_post_request(req);
    } else {
    	io_send_request(req);
    }

    return req->error;
}


int block_read(dev_t *dev, u32 block_start,u32 block_nums, void *buf)
{
	int ret;
	io_req_t *req = io_create_req_blk(dev, IO_READ, block_start, block_nums, buf);
	ret = io_request(req);
	io_free_req(req);
	return ret;
}

int block_write(dev_t *dev, u32 block_start, u32 block_nums, void *buf)
{
	int ret;
	io_req_t *req = io_create_req_blk(dev, IO_WRITE, block_start, block_nums, buf);
	ret = io_request(req);
	io_free_req(req);
	return ret;
}

