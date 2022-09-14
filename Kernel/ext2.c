/*
 * ext2.c
 *
 *  Created on: Oct 15, 2012
 *      Author: Shaun Yuan
 *      Copyright (c) 2012 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/ext2.h>
#include <kernel/io_dev.h>
#include <kernel/malloc.h>
#include <string.h>
#include <unistd.h>
#include <kernel/assert.h>
#include <kernel/time.h>
#include <kernel/types.h>
#include <kernel/stat.h>



extern struct timespec xtime;
#define CURRENT_TIME_SEC (xtime.ts_sec)

static struct file_ops ext2_file_ops;
vnode_t *ext2_rootv;

int ext2_create_entry(vnode_t *vnode, const char *path, int mode, vnode_t *new);
ext2_dir_t * ext2_dir_find_entry(vnode_t *pv, const char *path);

int ext2_alloc_block(vnode_t *vnode);


ext2_dir_t *
ext2_next_entry(ext2_dir_t *de)
{
	return (ext2_dir_t *)((char *)de + de->d_reclen);
}


int ext2_inode_to_block(ext2_fs_t *fs, int inode,
		int *containing_block, int *inode_offset)
{
	int block_group;
	int index;
	if (inode-- < 1){
		return -1;
	}

	block_group = inode / fs->inode_per_grp;
	index = inode % fs->inode_per_grp;

	*containing_block = fs->group_desc[block_group].inode_tbl_start
			+ (index * fs->inode_size) / fs->blksize;
	//inode offset in a block
	*inode_offset = index % (fs->blksize / fs->inode_size);
	return 0;
}



int ext2_rw_inode(vnode_t *vnode, int inode_no, void *buf, int mode)
{
	int block, offset;
	int retval;
	ext2_fs_t * fs;
	fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	ext2_i_t *tmp_buf = (ext2_i_t *)kmalloc(fs->blksize);
	assert(tmp_buf != NULL);

	ext2_inode_to_block(fs, inode_no, &block, &offset);
	LOG_INFO("inode(%d) is in the block(%d) offset(%d)", inode_no, block, offset);
	if (mode == 0){
		// read inode from disk
		retval = block_read(vnode->v_mp->dev,
				block * fs->blksize / 512, //sector number
				fs->blksize / 512,
				(void *)tmp_buf);
		if (retval < 0){
			LOG_ERROR("read inode(%d) failed", inode_no);
			kfree(tmp_buf);
			return -EIO;
		}

		memcpy(buf, (void *)&tmp_buf[offset * (fs->inode_size / sizeof(ext2_i_t))], fs->inode_size);

		LOG_INFO("read inode(%d) from disk ok", inode_no);
		kfree(tmp_buf);

	} else if (mode == 1){
		//write inode to disk
		retval = block_read(vnode->v_mp->dev,
				block * fs->blksize / 512,
				fs->blksize / 512,
				(void *)tmp_buf);

		if (retval < 0){
			LOG_ERROR("read inode(%d) error", inode_no);
			kfree(tmp_buf);
			return -EIO;
		}

		memcpy((void *)&tmp_buf[offset*(fs->inode_size / sizeof(ext2_i_t))], buf, fs->inode_size);

		retval = block_write(vnode->v_mp->dev,
				block * fs->blksize / 512,
				fs->blksize / 512,
				(void *)tmp_buf);

		if (retval < 0){
			LOG_ERROR("write inode to disk failed");
			kfree(tmp_buf);
			return -EIO;
		}
		LOG_INFO("write inode(%d) to disk ok", inode_no);
		kfree(tmp_buf);
	}

	return 0;
}

int ext2_rw_block(vnode_t *vnode, unsigned long block_no, void *buf, int mode)
{
	int retval;
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	if (mode == 0){

		retval = block_read(vnode->v_mp->dev,
				block_no * fs->blksize / 512,
				fs->blksize / 512,
				buf);

		if (retval == -1){
			LOG_ERROR("read block (%d) error", block_no);
			return -1;
		}

		//LOG_INFO("read block no(%d) ok", block_no);
	} else if (mode == 1){
		retval = block_write(vnode->v_mp->dev,
				block_no * fs->blksize / 512,
				fs->blksize / 512,
				buf);
		if (retval == -1){
			LOG_ERROR("write block(%d) error", block_no);
			return -1;
		}
		LOG_INFO("write block no(%d) ok", block_no);
	}
	return 0;
}
/*
 * get or put the block_no to an inode,
 */
int ext2_inode_rw_real_block(vnode_t *vnode, ext2_i_t *inode,
		int block_no, int *real_block, int mode)
{
	//unsigned long real_block;
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	int one_block_entries = fs->blksize >> 2;

	unsigned long singly_indirect_end = 12 + one_block_entries;
	unsigned long doubly_indirect_end = singly_indirect_end +
			one_block_entries * one_block_entries;
	unsigned long triply_indirect_end = doubly_indirect_end +
			one_block_entries * one_block_entries * one_block_entries;

	int retval;
	void *tmp_buf = NULL;

	tmp_buf = kmalloc(fs->blksize);
	assert(tmp_buf != NULL);

	if (block_no < 12){
		if (mode == 0){
			*real_block = inode->i_block[block_no];
		} else {
			inode->i_block[block_no] = *real_block;
		}
		kfree(tmp_buf);
		//LOG_INFO("real block no(%d)", *real_block);
		return *real_block;
	} else if (block_no < singly_indirect_end){
		if (inode->i_block[12] == 0){
			if (mode == 0){
				kfree(tmp_buf);
				*real_block = 0;
				return *real_block;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				inode->i_block[12] = rb;
				inode->i_sector_cnt += (fs->blksize / 512);
			}

		}
		retval = ext2_rw_block(vnode, inode->i_block[12], tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", inode->i_block[12]);
			kfree(tmp_buf);
			return -1;
		}
		if (mode == 0)
			*real_block = ((unsigned long *)tmp_buf)[block_no - 12];
		else {
			((unsigned long *)tmp_buf)[block_no - 12] = *real_block;
			retval = ext2_rw_block(vnode, inode->i_block[12], (void *)tmp_buf, 1);
			if (retval == -1){
				LOG_ERROR("write block(%d) error", inode->i_block[12]);
				kfree(tmp_buf);
				return -1;
			}
		}
		kfree(tmp_buf);
		tmp_buf = NULL;
		//LOG_INFO("real block no(%d)", *real_block);
		return *real_block;
	} else if (block_no < doubly_indirect_end) {
		int a = block_no - singly_indirect_end;
		int b = a / one_block_entries;
		int c = a - b * one_block_entries;
		if (inode->i_block[13] == 0){
			if(mode == 0){
				kfree(tmp_buf);
				*real_block = 0;
				return *real_block;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				inode->i_block[13] = rb;
				inode->i_sector_cnt += (fs->blksize / 512);
			}
		}
		retval = ext2_rw_block(vnode, inode->i_block[13], tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", inode->i_block[13]);
			kfree(tmp_buf);
			return -1;
		}

		int sblock = ((unsigned int *)tmp_buf)[b];
		if (sblock == 0){
			if (mode == 0){
				kfree(tmp_buf);
				*real_block = 0;
				return *real_block;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				sblock = rb;
				((unsigned int *)tmp_buf)[b] = rb;
				retval = ext2_rw_block(vnode, inode->i_block[13], tmp_buf, 1);
				if (retval == -1){
					LOG_ERROR("write block(%d) error", inode->i_block[13]);
					kfree(tmp_buf);
					return -1;
				}
				inode->i_sector_cnt += (fs->blksize / 512);
			}
		}
		retval = ext2_rw_block(vnode, sblock, tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", sblock);
			kfree(tmp_buf);
			return -1;
		}
		if (mode == 0)
			*real_block = ((unsigned int *)tmp_buf)[c];
		else {
			((unsigned int *)tmp_buf)[c] = *real_block;
			retval = ext2_rw_block(vnode, sblock, tmp_buf, 1);
			if (retval == -1){
				LOG_ERROR("write block(%d) error", sblock);
				kfree(tmp_buf);
				return -1;
			}
		}
		kfree(tmp_buf);
		//LOG_INFO("real block no(%d)", *real_block);
		return *real_block;
	} else if (block_no < triply_indirect_end){
		int a = block_no - doubly_indirect_end;
		int b = a / (one_block_entries * one_block_entries); // b is first block index
		int c = a - (b * one_block_entries * one_block_entries);
		int d = c / one_block_entries;	// d is second block index
		int e = c - d * one_block_entries;	// e is third block index
		if (inode->i_block[14] == 0){
			if (mode == 0){
				*real_block = 0;
				kfree(tmp_buf);
				return *real_block;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				inode->i_block[14] = rb;
				inode->i_sector_cnt += (fs->blksize / 512);
			}
		}
		retval = ext2_rw_block(vnode, inode->i_block[14], tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", inode->i_block[14]);
			kfree(tmp_buf);
			return -1;
		}

		int sblock = ((unsigned int *)tmp_buf)[b];
		if (sblock == 0) {
			if (mode == 0) {
				*real_block = 0;
				kfree(tmp_buf);
				return 0;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				sblock = rb;
				((unsigned int *)tmp_buf)[b] = rb;
				retval = ext2_rw_block(vnode, inode->i_block[14], tmp_buf, 1);
				if (retval == -1){
					LOG_ERROR("write block(%d) error", inode->i_block[14]);
					kfree(tmp_buf);
					return -1;
				}

				inode->i_sector_cnt += (fs->blksize / 512);
			}

		}
		retval = ext2_rw_block(vnode, sblock, tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", sblock);
			kfree(tmp_buf);
			return -1;
		}

		int sblock1 = ((unsigned int *)tmp_buf)[d];
		if (sblock1 == 0){
			if (mode == 0){
				*real_block = 0;
				kfree(tmp_buf);
				return *real_block;
			} else if (mode == 1){
				int rb = ext2_alloc_block(vnode);
				assert(rb != -1);
				sblock1 = rb;
				((unsigned int *)tmp_buf)[d] = rb;
				retval = ext2_rw_block(vnode, sblock, tmp_buf, 1);
				if (retval == -1){
					LOG_ERROR("write block(%d) error", sblock);
					kfree(tmp_buf);
					return -1;
				}
				inode->i_sector_cnt += (fs->blksize / 512);
			}
		}
		retval = ext2_rw_block(vnode, sblock1, tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", sblock);
			kfree(tmp_buf);
			return -1;
		}
		if (mode == 0)
			*real_block = ((unsigned int *)tmp_buf)[e];
		else if (mode == 1){
			((unsigned int *)tmp_buf)[e] = *real_block;
			retval = ext2_rw_block(vnode, sblock1, tmp_buf, 1);
			if (retval == -1){
				LOG_ERROR("write block(%d) error", sblock);
				kfree(tmp_buf);
				return -1;
			}
		}
		kfree(tmp_buf);
		//LOG_INFO("real block no(%d)", *real_block);
		return *real_block;
	}
	kfree(tmp_buf);
	LOG_INFO("block num too big");
	return -1;
}


int ext2_inode_read_block(vnode_t *vnode, ext2_i_t *inode, int block_no, void *buf)
{
	int retval = 0;
	int real_block;

	retval = ext2_inode_rw_real_block(vnode, inode, block_no, &real_block, 0);
	if (real_block == -1){
		LOG_ERROR("get real blcok number error");
		return -1;
	}
	if (real_block == 0) {
		LOG_INFO("real block is 0");
		return real_block;
	}


	retval = ext2_rw_block(vnode, real_block, buf, 0);
	if (retval == -1){
		LOG_ERROR("read block(%d) error", real_block);
		return -1;
	}

	return real_block;
}


int ext2_alloc_block(vnode_t *vnode)
{
	int block_no;
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	int i, offset, retval;
	char b;
	char *block_bitmap_buf = NULL;
	block_bitmap_buf = (char *)kmalloc(fs->blksize);
	assert(block_bitmap_buf != NULL);

	for (i=0; i<fs->groups_count; i++){
		if (fs->group_desc[i].free_block_nums > 0){
			retval = block_read(vnode->v_mp->dev,
					fs->group_desc[i].block_bitmap * fs->blksize / 512,
					fs->blksize / 512,
					(void *)block_bitmap_buf);
			if (retval == -1){
				LOG_ERROR("read block bitmap error");
				kfree(block_bitmap_buf);
				return -1;
			}

			offset = 0;
			while ((block_bitmap_buf[offset >> 3]) & (1 << (offset % 8)))
				offset++;

			block_no = offset + fs->blk_per_group * i;
			LOG_INFO("find free block:%d ", block_no);


			b = block_bitmap_buf[offset >> 3];
			b |= (1 << (offset % 8));
			block_bitmap_buf[offset >> 3] = b;
			--fs->group_desc[i].free_block_nums;
			--fs->sb->s_free_blocks;
			retval = block_write(vnode->v_mp->dev,
					fs->group_desc[i].block_bitmap * fs->blksize / 512,
					fs->blksize / 512,
					(void *)block_bitmap_buf);

			if (retval == -1){
				LOG_ERROR("write block bitmap error");
				kfree(block_bitmap_buf);
				return -1;
			}

			retval = block_write(vnode->v_mp->dev,
					fs->first_block_group_no * fs->blksize / 512,
					fs->block_group_desc_table_size / 512,
					(void *)fs->group_desc);

			if (retval == -1){
				LOG_ERROR("update first block group descriptor error");
				kfree(block_bitmap_buf);
				return -1;
			}

			break;
		}
	}


	return block_no;
}

int ext2_alloc_inode(vnode_t *vnode)
{
	ext2_fs_t *fs;
	unsigned int i, offset;
	int retval;
	int inode_no;
	fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	unsigned char *inode_bitmap_buffer = NULL;
	unsigned char b;
	inode_bitmap_buffer = (unsigned char *)kmalloc(fs->blksize);
	assert(inode_bitmap_buffer != NULL);

	for (i=0; i<fs->groups_count; i++){
		if (fs->group_desc[i].free_inode_nums > 0){
			//read inode bitmap on disk
			retval = block_read(vnode->v_mp->dev,
					fs->group_desc[i].inode_bitmap * fs->blksize / 512,
					fs->blksize / 512,
					(void *)inode_bitmap_buffer);

			if (retval == -1){
				LOG_ERROR("read inode bitmap error");
				kfree(inode_bitmap_buffer);
				return -1;
			}
			//check free inode bitmap
			offset = 0;
			while ((inode_bitmap_buffer[offset >> 3]) & (1 << (offset % 8)))
				offset++;

			inode_no = offset + fs->inode_per_grp * i + 1;
			LOG_INFO("find free inode:%d ", inode_no);

			//mark it as in use
			b = inode_bitmap_buffer[offset >> 3];
			b |= (1 << (offset % 8));
			inode_bitmap_buffer[offset >> 3] = b;
			--fs->group_desc[i].free_inode_nums;
			--fs->sb->s_free_inodes;
			//write it back into disk
			retval = block_write(vnode->v_mp->dev,
					fs->group_desc[i].inode_bitmap * fs->blksize / 512,
					fs->blksize / 512,
					(void *)inode_bitmap_buffer);

			if (retval == -1){
				LOG_ERROR("write inode bitmap error");
				kfree(inode_bitmap_buffer);
				return -1;
			}

			retval = block_write(vnode->v_mp->dev,
					fs->first_block_group_no * fs->blksize / 512,
					fs->block_group_desc_table_size / 512,
					(void *)fs->group_desc);

			if (retval == -1){
				LOG_ERROR("update first block group descriptor error");
				kfree(inode_bitmap_buffer);
				return -1;
			}

			break;

		}
	}

	kfree(inode_bitmap_buffer);
	return inode_no;

}

int ext2_free_block(vnode_t *vnode, int block_no)
{

	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;

	int group_no = (block_no - fs->first_data_block) / fs->blk_per_group;
	int offset_in_group = block_no - group_no * fs->blk_per_group;
	int byte_in_desc = offset_in_group >> 3;

	char *block_bitmap_buf = NULL;
	int retval;
	char b;
	block_bitmap_buf = (char *)kmalloc(fs->blksize);
	assert(block_bitmap_buf != NULL);
	LOG_INFO("freeing block %d", block_no);

	retval = block_read(vnode->v_mp->dev,
			fs->group_desc[group_no].block_bitmap * fs->blksize / 512,
			fs->blksize / 512,
			(void *)block_bitmap_buf);
	if (retval == -1){
		LOG_ERROR("read group(%d) block bitmap error", group_no);
		goto END;
	}


	b = block_bitmap_buf[byte_in_desc];
	b &= (~(1<<(offset_in_group % 8)));
	block_bitmap_buf[byte_in_desc] = b;

	retval = block_write(vnode->v_mp->dev,
			fs->group_desc[group_no].block_bitmap * fs->blksize / 512,
			fs->blksize / 512,
			(void *)block_bitmap_buf);

	if (retval == -1){
		LOG_ERROR("write group block bitmap error");
		goto END;
	}

	fs->group_desc[group_no].free_block_nums++;
	fs->sb->s_free_blocks++;
	retval = block_write(vnode->v_mp->dev,
			fs->first_block_group_no * fs->blksize / 512,
			fs->block_group_desc_table_size / 512,
			(void *)fs->group_desc);

	if (retval == -1){
		LOG_ERROR("write first block group desc error");
	}


END:
	kfree(block_bitmap_buf);
	return retval;
}

int ext2_dir_empty(vnode_t *vnode, ext2_i_t *inode, int inode_no)
{
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	char *block_buf = NULL;
	int retval, i, offset;
	int block_nr = 0;
	ext2_dir_t *de;

	if (inode->i_link_cnt > 2){
		return -1;
	}

	block_buf = (char *)kmalloc(fs->blksize);
	assert(block_buf != NULL);
AGAIN:
	offset = 0;
	retval = ext2_inode_read_block(vnode, inode, block_nr, block_buf);
	if (retval == 0 || retval == -1){
		goto END;
	}

	for (;i<inode->i_link_cnt; i++){
		de = (ext2_dir_t *)block_buf;
		if (de->d_inode != 0){
			if (de->d_name[0] != '.')
				goto not_empty;
			if (de->d_name_len > 2)
				goto not_empty;
			if (de->d_name_len < 2){
				if (de->d_inode != inode_no)
					goto not_empty;
			} else if (de->d_name[1] != '.')
				goto not_empty;

		}
		offset += de->d_reclen;
		//BugFix:2014.06.25
		//de = (ext2_dir_t *)((char *)de + EXT2_DIR_REAL_LEN(de->d_name_len));
		de = ext2_next_entry(de);
		if (offset > fs->blksize)
			goto AGAIN;
	}

	retval = 0;
	goto END;

not_empty:
	retval = -1;
END:
	kfree(block_buf);
	return retval;
}

int ext2_free_inode(vnode_t *vnode, int inode_no)
{
	ext2_fs_t *fs =(ext2_fs_t *)vnode->v_mp->fsdata;
	int group_no = (inode_no -1)/ fs->inode_per_grp;
	int offset_in_group = (inode_no -1) - group_no * fs->inode_per_grp;
	int byte_offset = offset_in_group >> 3;
	int retval, block_nr, real_block;
	char b;
	char *inode_bitmap_buf =  NULL;

	ext2_i_t *inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);

	retval = ext2_rw_inode(vnode, inode_no, (void *)inode, 0);
	if (retval == -1){
		LOG_ERROR("read inode(%d) failed", inode_no);
		goto END1;
	}
	//if this inode is a directory, check empty
	if (inode->i_mode & EXT2_FT_DIR){
		retval = ext2_dir_empty(vnode, inode, inode_no);
		if (retval == -1){
			LOG_ERROR("dir not empty %d", inode_no);
			goto END1;
		}
	}
	//free this inode's block(s) if it has
	block_nr = 0;
	for (;;){
		retval = ext2_inode_rw_real_block(vnode, inode, block_nr, &real_block, 0);
		if (retval == 0 || retval == -1)
			break;

		retval = ext2_free_block(vnode, real_block); // this is quite slow, i need a cache
		if (retval == -1){
			LOG_ERROR("free block(%d) failed", real_block);
		}

		block_nr++;
	}

	LOG_INFO("freeing inode %d", inode_no);
	inode_bitmap_buf = (char *)kmalloc(fs->blksize);
	assert(inode_bitmap_buf != NULL);

	retval = block_read(vnode->v_mp->dev,
			fs->group_desc[group_no].inode_bitmap * fs->blksize / 512,
			fs->blksize / 512,
			(void *)inode_bitmap_buf);

	if (retval == -1){
		LOG_ERROR("read group(%d) inode bitmap error", group_no);
		goto err_out;
	}


	b = inode_bitmap_buf[byte_offset];
	b &= (~(1<<(offset_in_group % 8)));
	inode_bitmap_buf[byte_offset] = b;

	retval = block_write(vnode->v_mp->dev,
			fs->group_desc[group_no].inode_bitmap *fs->blksize / 512,
			fs->blksize / 512,
			(void *)inode_bitmap_buf);

	if (retval == -1){
		LOG_ERROR("write group(%d) inode bitmap error",group_no);
		goto err_out;
	}

	++fs->group_desc[group_no].free_inode_nums;
	++fs->sb->s_free_inodes;
	retval = block_write(vnode->v_mp->dev,
			fs->first_block_group_no * fs->blksize / 512,
			fs->block_group_desc_table_size / 512,
			(void *)fs->group_desc);

	if (retval == -1){
		LOG_ERROR("write first block group desc error");
	}

err_out:
	kfree(inode_bitmap_buf);
END1:
	kfree(inode);
	return retval;
}

int ext2_dir_insert_entry(vnode_t *pv, int inode_no, const char *path)
{
	ext2_i_t *inode_buf = NULL;
	char *block_buf = NULL;
	ext2_fs_t *fs =(ext2_fs_t *)pv->v_mp->fsdata;
	int retval, real_len, real_block;
	int block_nr = 0;
	int name_len = strlen(path);
	int reclen = EXT2_DIR_REAL_LEN(name_len);
	char *kaddr;
	ext2_dir_t *de;

	inode_buf = (ext2_i_t *)kmalloc(fs->inode_size);
	if (inode_buf == NULL){
		LOG_ERROR("malloc for inode buffer error");
		return -ENOBUFS;
	}
	retval = ext2_rw_inode(pv, pv->v_id, (void *)inode_buf, 0);
	if (retval == -1){
		LOG_ERROR("read inode(%d) error", pv->v_id);
		kfree(inode_buf);
		return retval;
	}

	LOG_INFO("inode size:%d", inode_buf->i_size);
	block_buf = kmalloc(fs->blksize);
	if (block_buf == NULL){
		LOG_ERROR("malloc for block buffer error");
		kfree(inode_buf);
		return -ENOBUFS;
	}

	for (;;){

		real_block = ext2_inode_read_block(pv, inode_buf, block_nr, (void *)block_buf);
		if (real_block == 0){
			real_block = ext2_alloc_block(pv);
			//insert the real block into corresponding inode
			ext2_inode_rw_real_block(pv, inode_buf, block_nr, &real_block, 1);
			inode_buf->i_size += fs->blksize;	//in bytes
			inode_buf->i_sector_cnt += (fs->blksize / 512);
			ext2_rw_block(pv, real_block, (void *)block_buf, 0);
			((ext2_dir_t *)block_buf)->d_reclen = fs->blksize;
		}
		de = (ext2_dir_t *)block_buf;
		real_len = EXT2_DIR_REAL_LEN(de->d_name_len);
		kaddr = (char *)de;
		kaddr += (4096 - real_len);
		while ((char *)de <= kaddr){
			real_len = EXT2_DIR_REAL_LEN(de->d_name_len);
			if (strncmp(path, (char *)de->d_name, de->d_name_len) == 0){

				if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
					de->d_inode = 0;
					goto got_it;
				}

				LOG_ERROR("dir %s already exists", de->d_name);
				retval = -EEXIST;
				goto err_out; // -EEXIST
			}

			if (!de->d_inode && de->d_reclen > reclen)
				goto got_it;
			if (!de->d_inode && !de->d_reclen){
				//this should not be happen.
				//we got an empty directory without . and ..
				LOG_ERROR("corrupted file system.");
				de->d_reclen = 4096;
				ext2_rw_block(pv, real_block , (void *)block_buf, 1);
				ext2_dir_insert_entry(pv, inode_no, ".");
				ext2_dir_insert_entry(pv, pv->v_id, "..");

			}

			LOG_INFO("dir:%s real_len:%d d_reclen:%d reclen:%d",
					de->d_name, real_len, de->d_reclen, reclen);
			if (de->d_reclen > (reclen + real_len))
				goto got_it;

			//de = (ext2_dir_t *)((char *)de + real_len);
			de = ext2_next_entry(de);
		}
		block_nr++;
	}
err_out:
	//ext2_free_block(pv, real_block);
	kfree(inode_buf);
	kfree(block_buf);
	return retval;

got_it:
	if (de->d_inode){
		ext2_dir_t *de1 = (ext2_dir_t *)((char *)de + real_len);
		de1->d_reclen = de->d_reclen - real_len;
		de->d_reclen = real_len;
		de = de1;
	}
	de->d_inode = inode_no;
	strcpy((char *)de->d_name, path);
	de->d_name_len = name_len;
	//linux ext2 does not ensure that de->d_name[name_len] == 0
	de->d_name[name_len] = 0;
	LOG_INFO("create entry %s ok", de->d_name);
	inode_buf->i_link_cnt += 1;


	ext2_rw_block(pv, real_block , (void *)block_buf, 1);
	ext2_rw_inode(pv, pv->v_id, (void *)inode_buf, 1);

	kfree(inode_buf);
	kfree(block_buf);
	return 0;
}




int ext2_open(vnode_t *pv, const char *path, int mode, file_t **pfile)
{
	int retval;
	ext2_dir_t *de = NULL;
	vnode_t *vnode = NULL;
	ext2_i_t *inode;
	file_t *file;
	LOG_INFO("open file:%s", path);
	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;
	file = (file_t *)kzalloc(sizeof(file_t));
	assert(file != NULL);

	if (strcmp(path, "") == 0) {
		file->f_curr_pos = 0;
		file->f_end_pos = pv->v_size;
		file->f_type = pv->v_mode;
		file->f_ops = &ext2_file_ops;
		file->f_vnode = pv;
		file->f_refcnt++;
		*pfile = file;
		return 0;
	}

	de = ext2_dir_find_entry(pv, path);
	if (de == NULL){
		LOG_ERROR("open %s failed, can not find it", path);
		kfree(file);
		return -ENOENT;
	}

	vnode = vfs_alloc_vnode();
	assert(vnode != NULL);
	inode = kmalloc(fs->inode_size);
	assert(inode != NULL);
	retval = ext2_rw_inode(pv, de->d_inode, (void *)inode, 0);
	if (retval < -1){
		LOG_ERROR("read inode(%d) error", de->d_inode);
		kfree(inode);
		vfs_free_vnode(vnode);
		kfree(de);
		kfree(file);
		return -EIO;
	}
	vnode->v_atime = inode->i_atime = CURRENT_TIME_SEC;	//this should be updated
	vnode->v_ctime = inode->i_ctime;
	vnode->v_mtime = inode->i_mtime;
	vnode->v_gid = inode->i_gid;
	vnode->v_mode = inode->i_mode;
	vnode->v_id = de->d_inode;
	vnode->v_mp = pv->v_mp;
	vnode->v_device = pv->v_device;
	vnode->v_size = inode->i_size;




	//strcpy(vnode->v_mp->path_prefix, path);

	//write inode to disk, update access time
	//file->f_end_pos = pv->v_size;
	file->f_end_pos = inode->i_size;
	file->f_curr_pos = 0;
	file->f_ops = &ext2_file_ops;
	file->f_vnode = vnode;
	file->f_type = inode->i_mode;

	file->f_refcnt++;
	*pfile = file;

	kfree(de);
	kfree(inode);
	return 0;
}

int ext2_create(vnode_t *pv, const char *path, int mode, file_t **pfile)
{
	int retval;
	vnode_t *new_vnode = NULL;
	*pfile = NULL;

	new_vnode = vfs_alloc_vnode();
	assert(new_vnode != NULL);
	new_vnode->v_mp = pv->v_mp;
	new_vnode->v_device = pv->v_device;
	new_vnode->v_size = 0;

	retval = ext2_create_entry(pv, path, mode, new_vnode);
	if (retval < 0){
		LOG_ERROR("create entry %s failed", path);
		vfs_free_vnode(new_vnode);
		return retval;
	}

	file_t *file = (file_t *)kzalloc(sizeof(file_t));
	assert(file != NULL);

	file->f_curr_pos = 0;
	file->f_end_pos = new_vnode->v_size;
	file->f_ops = &ext2_file_ops;
	file->f_vnode = new_vnode;
	file->f_type = (mode_t)mode;
	file->f_refcnt++;
	*pfile = file;
	return 0;
}

/*
 * check parent inode type, permissions,
 * and whether the path exists or not
 */
int ext2_dir_check(vnode_t *pv, const char *path)
{
	int retval, i, block_nr = 0, offset = 0;
	ext2_fs_t *fs =(ext2_fs_t *)pv->v_mp->fsdata;
	char *block_buf = NULL;
	ext2_dir_t *de;
	ext2_i_t *inode_buf = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode_buf != NULL);
	retval = ext2_rw_inode(pv, pv->v_id, (void *)inode_buf, 0);
	if (retval < 0){
		LOG_ERROR("read inode(%d) error", pv->v_id);
		goto END;
	}

	if (!(inode_buf->i_mode & EXT2_FT_DIR)){
		LOG_ERROR("inode(%d) is not a dir", pv->v_id);
		retval = -ENOTDIR;
		goto END;
	}

	if (!(inode_buf->i_mode & U_WR)){
		LOG_ERROR("no permission to write inode(%d)", pv->v_id);
		retval = -EPERM;
		goto END;
	}

	block_buf = (char *)kmalloc(fs->blksize);
	assert(block_buf != NULL);
AGAIN:
	retval = ext2_inode_read_block(pv, inode_buf, block_nr, (void *)block_buf);
	if (retval <= 0){
		LOG_ERROR("read inode(%d) block error", pv->v_id);
		retval = -EIO;
		goto END1;
	}
	de = (ext2_dir_t *)block_buf;
	for (i=0; i<inode_buf->i_link_cnt; i++){
		if (de->d_inode == 0)
			break;

		if (strcmp((char *)de->d_name, (char *)path) == 0){
			LOG_ERROR("dir %s already exists in inode %d", path, pv->v_id);
			retval = -EEXIST;
			goto END1;
		}
		offset += de->d_reclen;
		//de = (ext2_dir_t *)((char *)de + EXT2_DIR_REAL_LEN(de->d_name_len));
		de = ext2_next_entry(de);
		if (offset > fs->blksize){
			block_nr++;
			goto AGAIN;
		}

	}

	retval = 0;

END1:
	kfree(block_buf);
END:
	kfree(inode_buf);
	return retval;
}

int ext2_create_directory(vnode_t *vnode, const char *path, vnode_t *new, int mode)
{
	ext2_i_t *inode;
	int  retval;
	int inode_no;
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;

	retval = ext2_dir_check(vnode, path);
	if (retval < 0){
		LOG_ERROR("dir check failed");
		return retval;
	}

	inode = (ext2_i_t *)kmalloc(fs->inode_size);
	if (inode == NULL){
		LOG_ERROR("malloc for inode buffer failed");
		return -ENOBUFS;
	}
	//first, alloc an inode number
	inode_no = ext2_alloc_inode(vnode);
	assert(inode_no != -1);
	//read the inode's content
	retval = ext2_rw_inode(vnode, inode_no, (void *)inode, 0);
	if (retval < 0){
		LOG_ERROR("read disk inode(%d) error", inode_no);
		goto err_out;
	}
	//this inode presents a directory, set inode mode and permissions
	inode->i_mode =(mode_t)mode; //((mode & 0xFFFF)| EXT2_FT_DIR);
	//inode->i_mode |= U_RWX;
	inode->i_sector_cnt = 0;
	inode->i_link_cnt = 0;	//this value was updated by insert_dir_entry
	inode->i_size = 0;
	inode->i_ctime = inode->i_mtime = inode->i_atime = CURRENT_TIME_SEC;

	retval = ext2_rw_inode(vnode, inode_no, (void *)inode, 1);
	if (retval < 0){
		LOG_ERROR("write inode to disk error");
		goto err_out;
	}

	//create entry in parent inode block
	retval = ext2_dir_insert_entry(vnode, inode_no, path);
	if (retval < 0)
		goto err_out;

	new->v_mp = vnode->v_mp;
	new->v_id = inode_no;
	new->v_size = inode->i_size;
	//insert entry in newer dir inode block
	retval = ext2_dir_insert_entry(new, inode_no, ".");	//self
	if (retval < 0)
		goto err_out;
	retval = ext2_dir_insert_entry(new, vnode->v_id, ".."); //parent
	if (retval < 0)
		goto err_out;

	kfree(inode);
	return 0;
err_out:
	kfree(inode);
	ext2_free_inode(vnode, inode_no);
	return retval;
}


int ext2_create_entry(vnode_t *vnode, const char *path, int mode, vnode_t *new)
{
	int retval;
	ext2_fs_t *fs = (ext2_fs_t *)vnode->v_mp->fsdata;
	ext2_i_t *inode;
	int inode_no = vnode->v_id;
	retval = ext2_dir_check(vnode, path);
	if (retval < 0){
		return retval;
	}

	if (mode & S_IFDIR){
		retval = ext2_create_directory(vnode, path, new, mode);
		if (retval < 0){
			LOG_ERROR("create directory %s failed", path);
			return retval;
		}

	} else {
		inode = (ext2_i_t *)kmalloc(fs->inode_size);
		assert(inode != NULL);
		inode_no = ext2_alloc_inode(vnode);
		assert(inode_no != -1);

		retval = ext2_rw_inode(vnode, inode_no, (void *)inode, 0);
		if (retval < 0){
			LOG_ERROR("read inode(%d) error", inode_no);
			kfree(inode);
			ext2_free_inode(vnode, inode_no);
			return retval;
		}
		if (mode & S_IFMT)
			mode |= EXT2_FT_REG;
		inode->i_mode = ((mode & 0xFFFF));
		inode->i_link_cnt = 1;
		inode->i_size = 0;
		inode->i_atime = inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
		inode->i_sector_cnt = 0;
		//inode->i_uid
		retval = ext2_dir_insert_entry(vnode, inode_no, path);
		if (retval < 0){
			LOG_ERROR("dir insert entry failed");
			kfree(inode);
			ext2_free_inode(vnode, inode_no);
			return retval;
		}

		retval = ext2_rw_inode(vnode, inode_no, (void *)inode, 1);
		if (retval < 0){
			LOG_ERROR("write inode(%s) to disk failed", inode_no);
			ext2_delete_entry(vnode, path);
			ext2_free_inode(vnode, inode_no);
			kfree(inode);
			return retval;
		}
		new->v_id = inode_no;
		new->v_mode = mode;
		new->v_size = 0;
		new->v_uid = new->v_gid = 0;

		kfree(inode);

	}

	LOG_INFO("create entry %s inode(%d)ok", path, inode_no);

	return 0;
}


ext2_dir_t * ext2_dir_find_entry(vnode_t *pv, const char *path)
{
	int retval, block_nr = 0;
	char *block_buf = NULL;
	ext2_i_t *inode = NULL;
	ext2_dir_t *d = NULL;
	ext2_dir_t *de;
	int  offset = 0;
	int len = strlen(path);

	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;
	inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);
	//LOG_INFO("finding entry:%s, pv->id:%d", path, pv->id);
	retval = ext2_rw_inode(pv, pv->v_id, (void *)inode, 0);
	if (retval == -1){
		LOG_ERROR("read inode error");
		goto END;
	}

	block_buf = (char *)kmalloc(fs->blksize);
	assert(block_buf != NULL);
AGAIN:
	memset(block_buf, 0, fs->blksize);
	retval = ext2_inode_read_block(pv, inode, block_nr, block_buf);
	if (retval == -1){
		LOG_ERROR("read inode(%d) block error", pv->v_id);
		goto END1;
	}
	if (retval == 0){
		goto END1;
	}
	de = (ext2_dir_t *)block_buf;
	//LOG_INFO("link count:%d", inode->i_link_cnt);
	for (;;){

		if (de->d_name_len != len)
			goto next;
		/*
		 * why we should use strncmp, not strcmp is because the
		 * fucking linux ext2 file system implementation,
		 * say, if you have a file named 'hello' under '/root' dir,and you
		 * use command 'mv /root/hello /root/init' to rename the file, and
		 * the fucking de->d_name is /root/inito on disk, not /root/init,
		 *  this is really ugly, and caused my file system crash once.
		 */
		if (memcmp((char *)de->d_name, (char *)path, de->d_name_len) == 0){
			d = (ext2_dir_t *)kmalloc(EXT2_DIR_REAL_LEN(de->d_name_len));
			assert(d != NULL);
			memcpy((void *)d, (void *)de, EXT2_DIR_REAL_LEN(de->d_name_len));
			//LOG_INFO("find item:%s", path);
			goto END1;
		}

		next:
		offset += de->d_reclen;
		LOG_INFO("offset:%d, d_name:%s, len:%d plen:%d", offset, de->d_name, de->d_name_len, len);
		//de = (ext2_dir_t *)((char *)de + EXT2_DIR_REAL_LEN(de->d_name_len));
		de = ext2_next_entry(de);
		if (offset >= fs->blksize){
			block_nr++;
			offset = 0;
			goto AGAIN;
		}

	}
	LOG_INFO("can not find %s in dir %d", path, pv->v_id);

END1:

	kfree(block_buf);
END:
	kfree(inode);
	return d;
}


int ext2_delete_entry(vnode_t *pv, const char *path)
{
	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;
	char *block_buf = NULL;
	ext2_dir_t *de;
	ext2_dir_t *pde = NULL;
	ext2_i_t *inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);
	int retval, i;
	int block_nr = 0;
	int block_no;
	int offset = 0;
	retval = ext2_rw_inode(pv, pv->v_id, (void *)inode, 0);
	if (retval == -1){
		LOG_ERROR("read inode error");
		goto END;
	}

	block_buf = (char *)kmalloc(fs->blksize);
	assert(block_buf != NULL);
AGAIN:
	retval = ext2_inode_read_block(pv, inode, block_nr, block_buf);
	if (retval == -1){
		LOG_ERROR("read inode(%d) block error", pv->v_id);
		retval = -1;
		goto END1;
	}
	if (retval == 0){
		goto END1;
	}
	de = (ext2_dir_t *)block_buf;
	for (i=0; i<inode->i_link_cnt; i++){

		if (strcmp((char *)de->d_name, (char *)path) == 0){
			block_no = retval;
			goto got_it;
		}
		offset += de->d_reclen;
		pde = de;
		de = (ext2_dir_t *)((char *)de + EXT2_DIR_REAL_LEN(de->d_name_len));

		if (offset > fs->blksize){
			block_nr++;
			goto AGAIN;
		}

	}
	LOG_INFO("can not find %s in dir %d", path, pv->v_id);
	goto END1;


got_it:
	if (pde){
		pde->d_reclen += de->d_reclen;
		--inode->i_link_cnt;

		retval = ext2_free_inode(pv, de->d_inode);
		if (retval == -1){
			LOG_ERROR("free inode(%d) failed", de->d_inode);
			goto END1;
		}

		ext2_rw_inode(pv, pv->v_id, inode, 1);

		memset((void *)de, 0, EXT2_DIR_REAL_LEN(de->d_name_len));

		ext2_rw_block(pv, block_no, block_buf, 1);
		retval = 0;
	}

END1:
	kfree(block_buf);
END:
	kfree(inode);
	return retval;
}

int ext2_delete(vnode_t *pv, const char *path)
{
	return ext2_delete_entry(pv, path);
}


int ext2_stat(vnode_t *pv, const char *path, struct stat *buf)
{
	int retval;
	ext2_dir_t *de = NULL;
	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;
	ext2_i_t *inode = NULL;

	if (strcmp(path, "") != 0) {
		de = ext2_dir_find_entry(pv, path);
		if (de == NULL){
			LOG_ERROR("file %s can not find", path);
			return -ENOENT;
		}
	}

	inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);
	if (de == NULL) {
		retval = ext2_rw_inode(pv, pv->v_id, (void *)inode, 0);
	} else {
		retval = ext2_rw_inode(pv, de->d_inode, (void *)inode, 0);
	}

	if (retval < 0){
		LOG_ERROR("read inode %d error", de->d_inode);
		goto END;
	}
	if (de == NULL)
		buf->st_ino = pv->v_id;
	else
		buf->st_ino = de->d_inode;

	buf->st_atime = inode->i_atime;
	buf->st_blksize = fs->blksize;
	buf->st_blocks = inode->i_sector_cnt;
	buf->st_ctime = inode->i_ctime;
	buf->st_dev =(devno_t)pv->v_mp->dev->devid;
	buf->st_gid = inode->i_gid;
	buf->st_mode = inode->i_mode;
	buf->st_mtime = inode->i_mtime;
	buf->st_nlink = inode->i_link_cnt;
	buf->st_size = inode->i_size;
	buf->st_uid = inode->i_uid;
	buf->st_rdev = 0;	// what does it used for?
	retval = 0;
END:
	if (de != NULL)
		kfree(de);
	LOG_INFO("out.retval:%d, mode %x %x.", retval, buf->st_mode, inode->i_mode);
	kfree(inode);
	return retval;

}




int ext2_lookup(struct vnode *pv, const char *path, struct vnode **new)
{
	ext2_dir_t *de = NULL;

	de = ext2_dir_find_entry(pv, path);
	if (de == NULL){
		return -ENOENT;
	}

	(*new)->v_id = de->d_inode;
	//LOG_INFO("item id:%d", de->d_inode);
	kfree(de);
	return 0;

}


int ext2_statfs(struct vnode *pv, struct statfs *sf)
{

	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;

	sf->f_bsize = fs->blksize;
	sf->f_type = fs->sb->s_signature;
	sf->f_bfree = fs->sb->s_free_blocks;
	sf->f_blocks = fs->sb->s_blocks_cnt;
	sf->f_files = fs->sb->s_inodes_cnt;
	sf->f_ffree = fs->sb->s_free_inodes;
	sf->f_flags = fs->sb->s_ext_features;

	memcpy((char *)&sf->f_fsid, (char *)fs->sb->s_fsid, sizeof(fsid_t));

	sf->f_owner = fs->sb->s_UID;
	sf->f_iosize =  fs->blksize;

	strcpy(sf->f_mntonname, pv->v_mp->path_prefix);

	return 0;
}

int ext2_sync(struct vnode *pv)
{
	int ret;
	ext2_fs_t *fs = (ext2_fs_t *)pv->v_mp->fsdata;
	ext2_i_t *inode = (ext2_i_t *)kzalloc(fs->inode_size);
	if (inode == NULL)
		return -ENOBUFS;

	ret = ext2_rw_inode(pv, pv->v_id, (void *)inode, 0);
	if (ret < 0){
		LOG_ERROR("read inode %d error.", pv->v_id);
		goto end;
	}

	inode->i_atime = pv->v_atime;
	inode->i_ctime = pv->v_ctime;
	inode->i_mtime = pv->v_mtime;
	inode->i_size = pv->v_size;
	inode->i_mode = pv->v_mode;
	inode->i_uid = pv->v_uid;
	inode->i_gid = pv->v_gid;

	ret = ext2_rw_inode(pv, pv->v_id, (void *)inode, 1);
	if (ret < 0){
		LOG_ERROR("write inode error.");
		goto end;
	}
	ret = 0;

end:
	if (inode)
		kfree(inode);
	return ret;
}

mount_ops_t ext2_mount_ops = {
		.lookup = ext2_lookup,
		.open = ext2_open,
		.create = ext2_create,
		.stat = ext2_stat,
		.statfs = ext2_statfs,
		.sync = ext2_sync,
		.delete = ext2_delete,
};


int ext2_read(file_t *file, void *buf, ulong_t nbytes)
{
	int retval = 0;
	ext2_fs_t *fs = (ext2_fs_t *)file->f_vnode->v_mp->fsdata;
	int block_start = file->f_curr_pos / fs->blksize;
	int count, buf_offset, block_offset, old_pos;
	char *tmp_buf = (char *)kmalloc(fs->blksize);
	assert(tmp_buf != NULL);

	ext2_i_t *inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);

	if (nbytes == 0)
		goto END;

	if (nbytes < 0 || buf == NULL || file == NULL){
		retval = -1;
		goto END;
	}

	if (nbytes + file->f_curr_pos >= file->f_vnode->v_size){
		count = file->f_vnode->v_size - file->f_curr_pos;
	} else {
		count = nbytes;
	}
	old_pos = file->f_curr_pos;
	int block_end = (count + file->f_curr_pos + fs->blksize -1) / fs->blksize;

	retval = ext2_rw_inode(file->f_vnode, file->f_vnode->v_id, (void *)inode, 0);
	if (retval == -1){
		LOG_ERROR("read inode(%d) error", file->f_vnode->v_id);
		goto END;
	}

	inode->i_atime = CURRENT_TIME_SEC;
	inode->i_mtime = CURRENT_TIME_SEC;
	//here, we must carefully code it,
	//for the data we need may cross the block boundary
	//what should we do if this inode has a hole?
	int block_nr = block_start;
	for (; block_nr < block_end; ){
		retval = ext2_inode_read_block(file->f_vnode, inode, block_nr, (void *)tmp_buf);
		if (retval == -1 ){
			LOG_ERROR("read inode block error");
			file->f_curr_pos = old_pos;
			goto END;
		}

		if (retval == 0){ // this  probably means a hole in file
			LOG_INFO("file may have a hole");
			memset((void *)tmp_buf, 0, fs->blksize);
		}
		buf_offset = file->f_curr_pos % fs->blksize;
		block_offset = fs->blksize - buf_offset;
		memcpy((void *)buf, (void *)&(tmp_buf[buf_offset]), count > block_offset ? block_offset : count);
		if (count > block_offset){
			file->f_curr_pos += block_offset;
			buf = (char *)buf + block_offset;
			count -= block_offset;
		}
		else {
			file->f_curr_pos += count;
			buf = (char *)buf + count;
			break;
		}
		block_nr++;
	}
	LOG_INFO("read inode(%d) data %d bytes from offset %d ok",
			file->f_vnode->v_id, file->f_curr_pos - old_pos, old_pos);
	//update atime and write it back to disk
	retval = ext2_rw_inode(file->f_vnode, file->f_vnode->v_id, (void *)inode, 1);
	if (retval < 0 ){
		LOG_ERROR("write indoe to disk failed");
		goto END;
	}



	retval = file->f_curr_pos - old_pos;

END:
	kfree(inode);
	kfree(tmp_buf);
	return retval;

}

int ext2_write(file_t *file, void *buf, ulong_t nbytes)
{
	ext2_fs_t *fs = (ext2_fs_t *)file->f_vnode->v_mp->fsdata;
	int block_start = file->f_curr_pos / fs->blksize;
	int block_needed = (nbytes + file->f_curr_pos + fs->blksize - 1) / fs->blksize;
	int i, block_no, retval, block_offset;
	int new_block_flag = 0;
	char *tmp_buf = NULL;
	if (nbytes == 0)
		return 0;
	if (nbytes == -1 || buf == NULL)
		return -1;

	tmp_buf = (char *)kmalloc(fs->blksize);
	assert(tmp_buf != NULL);
	memset((void *)tmp_buf, 0, fs->blksize);
	ext2_i_t *inode = (ext2_i_t *)kmalloc(fs->inode_size);
	assert(inode != NULL);

	retval = ext2_rw_inode(file->f_vnode, file->f_vnode->v_id, (void *)inode, 0);
	if (retval == -1){
		LOG_ERROR("read inode(%d) error", file->f_vnode->v_id);
		kfree(inode);
		kfree(tmp_buf);
		return -1;
	}

	for (i=0; i<block_needed; i++){
		retval = ext2_inode_rw_real_block(file->f_vnode, inode, block_start, &block_no, 0);
		if (retval == 0){
			block_no = ext2_alloc_block(file->f_vnode);
			if (block_no == -1){
				LOG_ERROR("alloc block failed");
				goto END;
			}
			new_block_flag = 1;
			memset((void *)tmp_buf, 0, fs->blksize);
			goto block_ready;
		} else if (retval == -1){
			goto END;
		}

		retval = ext2_rw_block(file->f_vnode, block_no, (void *)tmp_buf, 0);
		if (retval == -1){
			LOG_ERROR("read block(%d) error", block_no);
			goto END;
		}
block_ready:
		block_offset = file->f_curr_pos % fs->blksize;

		memcpy((void *)&(tmp_buf[block_offset]), buf,
				(nbytes + block_offset) >= fs->blksize ? (fs->blksize - block_offset) : nbytes);
		retval = ext2_rw_block(file->f_vnode, block_no, (void *)tmp_buf, 1);
		if (retval == -1){
			LOG_ERROR("write block(%d) error", block_no);
			//FIXME:if we need three blocks to store data, and when we write
			//the third block failed, we should free the first and second block
			//allocated earlier
			goto END1;
		}
		if (new_block_flag){
			retval = ext2_inode_rw_real_block(file->f_vnode, inode, block_start, &block_no, 1);
			if (retval == -1){
				LOG_ERROR("set inode(%d) real block(%d) error,",file->f_vnode->v_id, block_no);
				goto END1;
			}
		}

		if ((nbytes + block_offset) >= fs->blksize){
			file->f_curr_pos += (fs->blksize - block_offset);
			nbytes -= (fs->blksize - block_offset);
			buf = (char *)buf + (fs->blksize - block_offset);
			if (new_block_flag){
				inode->i_sector_cnt += (fs->blksize / 512);
				inode->i_size += (block_start * fs->blksize + fs->blksize - block_offset);
			}

		} else {
			file->f_curr_pos += nbytes;
			buf = (char *)buf + nbytes;
			if (new_block_flag) {
				inode->i_sector_cnt += (fs->blksize / 512);
				inode->i_size += (block_start * fs->blksize + nbytes);
			} else
				inode->i_size += nbytes;

			break;
		}
		block_start++;
	}

	inode->i_atime = inode->i_mtime = CURRENT_TIME_SEC;
	retval = ext2_rw_inode(file->f_vnode, file->f_vnode->v_id, (void *)inode, 1);
	if (retval == -1){
		LOG_ERROR("update inode(%d) failed", file->f_vnode->v_id);
		goto END1;

	}
	file->f_end_pos = inode->i_size;
	file->f_vnode->v_size = inode->i_size;
	retval = 0;
	goto END;
END1:
	ext2_free_block(file->f_vnode, block_no);
END:
	kfree(tmp_buf);
	kfree(inode);
	return retval;

}

int ext2_seek(file_t *file, off_t pos, int whence)
{
	if (whence == SEEK_SET){
		if (pos < 0)
			return -EINVAL;
		file->f_curr_pos = pos;

	} else if (whence == SEEK_END) {
		file->f_curr_pos = (file->f_end_pos + pos);
	} else if (whence == SEEK_CUR){
		file->f_curr_pos += pos;
	} else {
		return -1;
	}
	return file->f_curr_pos;
}

int ext2_fstat(file_t *file, struct stat *buf)
{
	ext2_fs_t *fs = (ext2_fs_t *)file->f_vnode->v_mp->fsdata;
	buf->st_atime = file->f_vnode->v_atime;
	buf->st_blksize = fs->blksize;
	buf->st_ino = file->f_vnode->v_id;
	buf->st_mode = file->f_vnode->v_mode;
	buf->st_mtime = file->f_vnode->v_mtime;
	buf->st_size = file->f_vnode->v_size;
	buf->st_gid = file->f_vnode->v_gid;
	buf->st_dev = file->f_vnode->v_device;

	return 0;
}

int ext2_close(file_t *file)
{
	if (file->f_refcnt > 0)
		file->f_refcnt--;
	if (file->f_refcnt == 0){
		//call ext2_sync, update data to disk
		if (file->f_vnode->v_id != 2)
			kfree(file->f_vnode);
		kfree(file);
	}

	return 0;
}

static struct file_ops ext2_file_ops = {
		.read = ext2_read,
		.write = ext2_write,
		.fstat = ext2_fstat,
		.seek = ext2_seek,
		.close = ext2_close
};

int ext2_mount(vnode_t *vnode)
{
	int retval = -1;
	ext2_sb_t *sb = NULL;
	ext2_fs_t *fs = NULL;
	int block_group_desc_table_size;
	int first_block_group_no;
	int root_inode_block;
	int root_inode_index;
	ext2_i_t *root = NULL;
	LOG_INFO("ext2 mount in");
	sb = (ext2_sb_t *)kmalloc(sizeof(ext2_sb_t));
	if (sb == NULL){
		LOG_ERROR("kmalloc for super block failed");
		return retval;
	}

	/*
	 * here, we should check the device's block size,
	 * then read the super block,
	 * i assume that the device's block size is 512bytes
	 */
	if (block_read(vnode->v_mp->dev, 2, 2, (void *)sb) < 0){
		LOG_ERROR("read super block failed");
		goto END1;
	}

	if (sb->s_signature != EXT2_MAGIC){
		LOG_ERROR("trying to mount an invalid ext2 file system");
		goto END1;
	}

	LOG_INFO("blocks:%d inodes:%d blocks per group:%d inodes per group:%d",
			sb->s_blocks_cnt, sb->s_inodes_cnt, sb->s_blocks_per_group,
			sb->s_inodes_per_group);

	fs = (ext2_fs_t *)kmalloc(sizeof(ext2_fs_t));
	if (fs == NULL){
		LOG_ERROR("malloc for ext2 file system struct failed");
		goto END2;
	}

	fs->blk_per_group = sb->s_blocks_per_group;
	fs->inode_per_grp = sb->s_inodes_per_group;
	fs->blksize = 1024 << sb->s_log_block_size;
	fs->inode_size = sb->s_inode_size;
	fs->groups_count = ((sb->s_blocks_cnt + sb->s_first_data_block -1) / (sb->s_blocks_per_group)) + 1;
	LOG_INFO("block size:%d block groups:%d volume:%s", fs->blksize, fs->groups_count,
			sb->s_volume_name);
	LOG_INFO("ext2 major version:%d minor version:%d", sb->s_major, sb->s_minor);
	block_group_desc_table_size = sizeof(ext2_bgd_t) * fs->groups_count;

	assert(block_group_desc_table_size <= fs->blksize);

	if (block_group_desc_table_size % 512/*this should be dev block size*/ != 0){
		block_group_desc_table_size += (512 - block_group_desc_table_size % 512);
	}
	LOG_INFO("block group desc_table size %d", block_group_desc_table_size);
	fs->group_desc = (ext2_bgd_t *)kmalloc(block_group_desc_table_size);
	if (fs->group_desc == NULL){
		LOG_ERROR("kmalloc for block group descriptor failed");
		goto END2;
	}

	first_block_group_no = sb->s_first_data_block + 1;
	if (block_read(vnode->v_mp->dev,
			first_block_group_no * fs->blksize / 512,
			block_group_desc_table_size / 512,
			(void *)fs->group_desc) < 0){
		LOG_ERROR("read block group descriptor error");
		goto END3;
	}

	fs->first_block_group_no = first_block_group_no;
	fs->first_data_block = sb->s_first_data_block;
	LOG_INFO("in first block group descriptor:\n " \
			"block_bitmap:%d inode_bitmap:%d " \
			"inode_tbl_start:%d", fs->group_desc[0].block_bitmap,
			fs->group_desc[0].inode_bitmap,
			fs->group_desc[0].inode_tbl_start);

	root = (ext2_i_t *)kmalloc(fs->inode_size > 512 ? fs->inode_size : 512);
	if (root == NULL){
		LOG_ERROR("malloc failed for root inode");
		goto END3;
	}

	ext2_inode_to_block(fs, 2, &root_inode_block, &root_inode_index);
	LOG_INFO("root inode block:%d root inode index:%d", root_inode_block,
			root_inode_index);

	if (block_read(vnode->v_mp->dev,
			root_inode_block * fs->blksize / 512, // physical sector number on device
			1,
			(void *)root) == -1){
		LOG_ERROR("read root inode failed");
		goto END4;
	}

	fs->root_inode = (ext2_i_t *)kmalloc(fs->inode_size);
	if (fs->root_inode == NULL){
		LOG_ERROR("malloc for root dir failed");
		goto END4;
	}
	memcpy(fs->root_inode,(void *)&root[fs->inode_size/sizeof(ext2_i_t)], fs->inode_size);
	LOG_INFO("root inode size:%d", fs->root_inode->i_size);
	vnode->v_id = 2;
	vnode->v_mode = fs->root_inode->i_mode;
	vnode->v_atime = fs->root_inode->i_atime;
	vnode->v_mtime = fs->root_inode->i_mtime;
	vnode->v_ctime = fs->root_inode->i_ctime;
	vnode->v_size = fs->root_inode->i_size;
	vnode->v_gid = fs->root_inode->i_gid;
	vnode->v_mp->fsdata = (void *)fs;
	vnode->v_mp->mp_ops = &ext2_mount_ops;
	vnode->v_mp->vnode = ext2_rootv;
	//vnode->fops = &ext2_file_ops;
	//vnode->priv_data = (void *)ext2_rootv;
	fs->sb = sb;
	kfree(root);
	//kfree(sb);
	retval = 0;
	return retval;
END4:
	kfree(root);
END3:
	kfree(fs->group_desc);
END2:
	kfree(fs);
END1:
	kfree(sb);
	return retval;

}


static struct file_system_ops ext2_fs_ops = {
	.format = NULL,
	.mount = ext2_mount,
};


int ext2_fs_init()
{
	register_filesystem("ext2", &ext2_fs_ops);
	return vfs_mount("sata", "/", "ext2", &ext2_rootv);
}




