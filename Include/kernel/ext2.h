/*
 * ext2.h
 *
 *  Created on: Oct 15, 2012
 *      Author: root
 */

#ifndef SHAUN_EXT2_H_
#define SHAUN_EXT2_H_

#include <kernel/kernel.h>
#include <kernel/vfs.h>
/*
 * super block fields
0	 3	 4	 Total number of inodes in file system
4	 7	 4	 Total number of blocks in file system
8	 11	 4	 Number of blocks reserved for superuser (see offset 80)
12	 15	 4	 Total number of unallocated blocks
16	 19	 4	 Total number of unallocated inodes
20	 23	 4	 Block number of the block containing the superblock
24	 27	 4	log2 (block size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the block size)
28	 31	 4	log2 (fragment size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the fragment size)
32	 35	 4	 Number of blocks in each block group
36	 39	 4	 Number of fragments in each block group
40	 43	 4	 Number of inodes in each block group
44	 47	 4	 Last mount time (in POSIX time)
48	 51	 4	 Last written time (in POSIX time)
52	 53	 2	 Number of times the volume has been mounted since its last consistency check (fsck)
54	 55	 2	 Number of mounts allowed before a consistency check (fsck) must be done
56	 57	 2	 Ext2 signature (0xef53), used to help confirm the presence of Ext2 on a volume
58	 59	 2	 File system state (see below)
60	 61	 2	 What to do when an error is detected (see below)
62	 63	 2	 Minor portion of version (combine with Major portion below to construct full version field)
64	 67	 4	POSIX time of last consistency check (fsck)
68	 71	 4	 Interval (in POSIX time) between forced consistency checks (fsck)
72	 75	 4	 Operating system ID from which the filesystem on this volume was created (see below)
76	 79	 4	 Major portion of version (combine with Minor portion above to construct full version field)
80	 81	 2	 User ID that can use reserved blocks
82	 83	 2	 Group ID that can use reserved blocks

extend flags
start end size 		Field Description
84	 87	 4	 First non-reserved inode in file system. (In versions < 1.0, this is fixed as 11)
88	 89	 2	 Size of each inode structure in bytes. (In versions < 1.0, this is fixed as 128)
90	 91	 2	 Block group that this superblock is part of (if backup copy)
92	 95	 4	 Optional features present (features that are not required to read or write, but usually result in a performance increase. see below)
96	 99	 4	 Required features present (features that are required to be supported to read or write. see below)
100	 103	 4	 Features that if not supported, the volume must be mounted read-only see below)
104	 119	 16	 File system ID (what is output by blkid)
120	 135	 16	 Volume name (C-style string: characters terminated by a 0 byte)
136	 199	 64	 Path volume was last mounted to (C-style string: characters terminated by a 0 byte)
200	 203	 4	 Compression algorithms used (see Required features above)
204	 204	 1	 Number of blocks to preallocate for files
205	 205	 1	 Number of blocks to preallocate for directories
206	 207	 2	 (Unused)
208	 223	 16	 Journal ID (same style as the File system ID above)
224	 227	 4	 Journal inode
228	 231	 4	 Journal device
232	 235	 4	 Head of orphan inode list
236	 1023	 X	 (Unused)

 */
struct ext2_super_block {
	unsigned long s_inodes_cnt;
	unsigned long s_blocks_cnt;
	unsigned long s_block_reserved;
	unsigned long s_free_blocks;
	unsigned long s_free_inodes;
	unsigned long s_first_data_block;
	unsigned long s_log_block_size;
	unsigned long s_log_fragment_size;
	unsigned long s_blocks_per_group;
	unsigned long s_fragment_per_group;
	unsigned long s_inodes_per_group;
	unsigned long s_last_mount_time;
	unsigned long s_last_written_time;
	unsigned short s_mounted_times;
	unsigned short s_allowed_times;
	unsigned short s_signature;
	unsigned short s_state;
	unsigned short s_on_error;
	unsigned short s_minor;
	unsigned long s_fsck_time;
	unsigned long s_interval;
	unsigned long s_osid;
	unsigned long s_major;
	unsigned short s_UID;
	unsigned short s_GID;
	unsigned long s_firt_inodes;
	unsigned short s_inode_size;
	unsigned short s_owner_bg;
	unsigned long s_opt_features;
	unsigned long s_rqd_features;
	unsigned long s_ext_features;
	unsigned long s_fsid[4];
	unsigned char s_volume_name[16];
	unsigned char s_path_last_mounted[64];
	unsigned long s_comp_algorithms;
	unsigned char s_prealloc_blocks;
	unsigned char s_prealloc_dir_blocks;
	unsigned short s_unused;
	unsigned long s_journalid[4];
	unsigned long s_journal_inode;
	unsigned long s_journal_device;
	unsigned long s_orphan_inode_head;
	unsigned char s_padding[788];
};
typedef struct ext2_super_block ext2_sb_t;


/*
 *
0	 3	 4	 Block address of block usage bitmap
4	 7	 4	 Block address of inode usage bitmap
8	 11	 4	 Starting block address of inode table
12	 13	 2	 Number of unallocated blocks in group
14	 15	 2	 Number of unallocated inodes in group
16	 17	 2	 Number of directories in group
18	 31	 X	 (Unused)
 *
 */

struct ext2_block_group_descriptors {
	unsigned long block_bitmap;
	unsigned long inode_bitmap;
	unsigned long inode_tbl_start;
	unsigned short free_block_nums;
	unsigned short free_inode_nums;
	unsigned short dir_nums;
	unsigned char padding[14];
};
typedef struct ext2_block_group_descriptors ext2_bgd_t;


/*
 *
0	 1	 2	 Type and Permissions (see below)
2	 3	 2	 User ID
4	 7	 4	 Lower 32 bits of size in bytes
8	 11	 4	 Last Access Time (in POSIX time)
12	 15	 4	 Creation Time (in POSIX time)
16	 19	 4	 Last Modification time (in POSIX time)
20	 23	 4	 Deletion time (in POSIX time)
24	 25	 2	 Group ID
26	 27	 2	 Count of hard links (directory entries) to this inode. When this reaches 0, the data blocks are marked as unallocated.
28	 31	 4	 Count of disk sectors (not Ext2 blocks) in use by this inode, not counting the actual inode structure nor directory entries linking to the inode.
32	 35	 4	 Flags (see below)
36	 39	 4	Operating System Specific value #1
40	 43	 4	 Direct Block Pointer 0
44	 47	 4	 Direct Block Pointer 1
48	 51	 4	 Direct Block Pointer 2
52	 55	 4	 Direct Block Pointer 3
56	 59	 4	 Direct Block Pointer 4
60	 63	 4	 Direct Block Pointer 5
64	 67	 4	 Direct Block Pointer 6
68	 71	 4	 Direct Block Pointer 7
72	 75	 4	 Direct Block Pointer 8
76	 79	 4	 Direct Block Pointer 9
80	 83	 4	 Direct Block Pointer 10
84	 87	 4	 Direct Block Pointer 11
88	 91	 4	 Singly Indirect Block Pointer (Points to a block that is a list of block pointers to data)
92	 95	 4	 Doubly Indirect Block Pointer (Points to a block that is a list of block pointers to Singly Indirect Blocks)
96	 99	 4	 Triply Indirect Block Pointer (Points to a block that is a list of block pointers to Doubly Indirect Blocks)
100	 103	 4	 Generation number (Primarily used for NFS)
104	 107	 4	 In Ext2 version 0, this field is reserved. In version >= 1, Extended attribute block (File ACL).
108	 111	 4	 In Ext2 version 0, this field is reserved. In version >= 1, Upper 32 bits of file size (if feature bit set) if it's a file, Directory ACL if it's a directory
112	 115	 4	 Block address of fragment
116	 127	 12	Operating System Specific Value #2
 */

typedef struct ext2_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned long i_mtime;
	unsigned long i_dtime;
	unsigned short i_gid;
	unsigned short i_link_cnt;
	unsigned long i_sector_cnt;
	unsigned long i_flags;
	unsigned long i_osd1;
	unsigned long i_block[15];
	unsigned long i_generation;
	unsigned long i_acl;
	unsigned long i_diracl;
	unsigned long i_frag_block;
	unsigned long i_osd2[3];
} ext2_i_t;



/*
 *
0	 3	 4	 Inode
4	 5	 2	 Total size of this entry (Including all subfields)
6	 6	 1	 Name Length least-significant 8 bits
7	 7	 1	Type indicator (only if the feature bit for "directory entries have file type byte" is set, else this is the most-significant 8 bits of the Name Length)
8	 8+N-1	 N	 Name characters
 */

typedef struct ext2_dir_entry {
	unsigned long d_inode;
	unsigned short d_reclen;
	unsigned char d_name_len;
	unsigned char d_type;
	unsigned char d_name[255];
} ext2_dir_t;




struct ext2_file_system	{
	unsigned long blksize;
	unsigned long inode_size;
	unsigned long groups_count;
	unsigned long blk_per_group;
	unsigned long inode_per_grp;
	unsigned long first_block_group_no;
	unsigned long first_data_block;
	unsigned long block_group_desc_table_size;
	ext2_bgd_t	*group_desc;
	ext2_i_t *root_inode;
	ext2_sb_t *sb;
};
typedef struct ext2_file_system ext2_fs_t;

// file system state
#define FILE_SYSTEM_CLEAN	1
#define FILE_SYSTEM_ERROR	2
// error handling methods
#define ERROR_IGNORE		1
#define ERROR_REMOUNT		2
#define ERROR_KERNEL_PANIC	3
// OS IDS
#define CREATOR_OS_LINUX	0
#define CREATOR_OS_HURD		1
#define CREATOR_OS_MASIX	2
#define CREATOR_OS_FREEBSD	3
#define CREATOR_OS_OTHER	4
#define CREATOR_OS_SHAUN	5
//OPTIONAL FEATURE FLAGS
#define PREALLOC_BLOCK_FOR_DIR	0x01
#define AFS_INODES_EXIST		0x02
#define FS_JOURNAL				0x04
#define INODES_EXT_ATTRI		0x08
#define FS_RESIZE				0x10
#define DIR_HASH_INDEX			0x20
//required feature flags
#define COMPRESSION_USED		0x01
#define DIR_WITH_TYPE			0x02
#define FS_REPLAY_JOURNAL		0x04
#define FS_JOURNAL_DEVICE		0x08
//read only features flags
#define SPARSE_SB				0x01
#define FS_LARGE_SIZE			0x02
#define DIR_BIN_TREE			0x03
// inode type and permissions
#define EXT2_FT_FIFO			0x1000
#define EXT2_FT_CHRDEV			0x2000
#define EXT2_FT_DIR				0x4000
#define EXT2_FT_BLKDEV			0x6000
#define EXT2_FT_REG				0x8000
#define EXT2_FT_SYM				0xA000
#define EXT2_FT_SOCK			0xC000
#define EXT2_FT_IFMT			0xF000

#define O_EXE	0x001
#define O_WR	0x002
#define O_RD	0x004
#define G_EXE	0x008
#define G_WR	0x010
#define G_RD	0x020
#define U_EXE	0x040
#define U_WR	0x080
#define U_RD	0x100
#define STICK	0x200
#define SETGID	0x400
#define SEGUID	0x800
#define U_RWX	(U_EXE | U_WR | U_RD)
#define O_RWX	(O_EXE | O_WR | O_RD)
#define G_RWX	(G_EXE | G_WR | G_RD)

// inode flags
#define	SEC_DEL			0x00000001
#define DEL_CPY			0x00000002
#define FILE_COMP		0x00000004
#define SYN_UPDATES		0x00000008
#define IMMUTABLE_FILE 	0x00000010
#define APPEND_ONLY		0x00000020
#define EX_DUMP_COMMAND	0x00000040
#define NO_UPDT_ATIME	0x00000080
#define HASH_INDEX_DIR	0x00010000
#define AFS_DIR			0x00020000
#define JOURNAL_FIEL	0x00040000

#define EXT2_DIR_REAL_LEN(name_len)	(((name_len) + 11) & ~3)

#define EXT2_MAGIC		0xef53

int ext2_fs_init();
int ext2_mount(vnode_t *vnode);
int ext2_create(vnode_t *pv, const char *path, int mode, file_t **pfile);
int ext2_open(vnode_t *pv, const char *path, int mode, file_t **pfile);
int ext2_write(file_t *file, void *buf, ulong_t nbytes);
int ext2_read(file_t *file, void *buf, ulong_t nbytes);
int ext2_seek(file_t *file, off_t pos, int whence);
int ext2_create_directory(vnode_t *vnode, const char *path, vnode_t *new, int mode);
int ext2_create_entry(vnode_t *vnode, const char *path, int mode, vnode_t *new);
int ext2_delete_entry(vnode_t *pv, const char *path);
ext2_dir_t * ext2_dir_find_entry(vnode_t *pv, const char *path);
#endif /* SHAUN_EXT2_H_ */



