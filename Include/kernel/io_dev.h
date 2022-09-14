/*
 * block_dev.h
 *
 * Copyright (c) 2012 Shaun Yuan
 * 
 */

#ifndef SHAUN_BLOCK_DEV_H_
#define SHAUN_BLOCK_DEV_H_

#include <list.h>
#include <kernel/types.h>

#define DEV_MAX_NAME_LEN 15
#define MKDEVID(maj, min)	((maj)<<16|(min))
#define DEV_MAJOR(x) ((x & 0xffff0000) >> 16)
#define DEV_MINOR(x) (x & 0xffff)

struct vds;
struct _io_request;

struct dev_ops {
    int (*open)(struct vds *dev);
    int (*read)(struct vds *dev, void *buf, size_t size);
    int (*write)(struct vds *dev, void *buf, size_t size);
    int (*io_request)(struct _io_request *req);
    int (*close)(struct vds *dev);
    //int (*get_block_nums)(struct vds *dev);
};
typedef struct dev_ops dev_ops_t;

typedef struct wait_queue_head {
	unsigned long lock;
	struct linked_list waiter;
} wait_queue_head_t;


struct vds {
    char *name;
    char inuse;
    int devid;
    struct dev_ops *ops;
    struct linked_list	dev_list;
    void *driver_data;
};
typedef struct vds dev_t;

enum request_type {
	IO_READ = 0,
	IO_WRITE,
	IO_COMMAND
};
typedef enum request_tyte io_req_type_t;

enum request_state {
	IO_PENDING = 1,
	IO_COMPLETED,
	IO_ERROR
};
typedef enum request_stat io_state_t;


#define IO_TARGET_BLOCK_SHIFT	1UL
#define IO_TARGET_CHAR_SHIFT	2UL
#define IO_TARGET_CMD_SHIFT		3UL
#define IO_AYSNC_SHIFT			4UL

#define IO_TARGET_BLOCK			(1<<(IO_TARGET_BLOCK_SHIFT))
#define IO_TARGET_CHAR			(1<<(IO_TARGET_CHAR_SHIFT))
#define IO_TARGET_CMD			(1<<(IO_TARGET_CMD_SHIFT))
#define IO_AYSNC				(1<<(IO_AYSNC_SHIFT))
struct wait_queue_head;
struct _io_request {
	dev_t *dev;
	enum request_type io_req_type;
	unsigned long flag;
	union {
		struct _block_req {
			unsigned long block_start;
			unsigned long block_nums;
		}b_req;

		struct _char_req {
			unsigned long nr_bytes;
			unsigned long offset;
		}c_req;

		struct _cmd_req {
			unsigned long cmd;
			unsigned long pad;
		}cmd_req;

	} _u;
	void *buf;
	enum request_state io_state;
	int error;
	struct linked_list list;
	struct wait_queue_head waiter;

};
typedef struct _io_request io_req_t;


struct driver;

struct driver_ops {
	int (*driver_init)(dev_t *dev, struct driver *drv);
	int (*driver_unload)(dev_t *dev, struct driver *drv);
};


struct driver {
	dev_t *dev;
	char *name;
	struct driver_ops drv_ops;
	u8 ref_cnt;
	u32 flag;
	struct linked_list dlist;
};


extern io_req_t *
io_create_req_chr(dev_t *dev, enum request_type type,
		u32 nr_bytes, void *buf);

extern io_req_t *io_create_req_blk(dev_t *dev, enum request_type type,
    u32 block_start, u32 block_nums, void *buf);

extern void io_wait(wait_queue_head_t *wq);
extern void* io_schedule(void * args);
extern void io_wakeup(wait_queue_head_t *wq);
extern int io_request(io_req_t *req);
extern void io_free_req(io_req_t *req);
extern int register_device(const char *name, dev_ops_t *ops, int devid, void *data);
extern int open_device(const char *devname, dev_t **pdev);
extern int close_device(dev_t *dev);
extern int block_read(dev_t *dev, u32 block_start, u32 block_nums, void *buf);
extern int block_write(dev_t *dev, u32 block_start, u32 block_nums, void *buf);
#endif /* SHAUN_BLOCK_DEV_H_ */


