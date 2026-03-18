/*
 * mmapfs.h - MmapFS: A userspace filesystem example built on mmap
 *
 * This file defines all on-disk structures, in-memory structures,
 * and function prototypes for the mmapfs example filesystem.
 *
 * Disk Layout:
 *   Block 0:           Superblock
 *   Blocks 1..J:       Journal (write-ahead log)
 *   Block J+1:         Inode bitmap
 *   Blocks J+2..J+2+D: Data bitmap
 *   Blocks ..:         Inode table
 *   Blocks ..:         Data blocks
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#ifndef MMAPFS_H
#define MMAPFS_H

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ============================================================
 * Constants
 * ============================================================ */

#define MMAPFS_BLOCK_SIZE 4096
#define MMAPFS_BLOCK_SHIFT 12

/* Magic numbers */
#define MMAPFS_MAGIC 0x4D4D4150          /* "MMAP" */
#define MMAPFS_JOURNAL_MAGIC 0x4A524E4C  /* "JRNL" */

/* Inode constants */
#define MMAPFS_INODE_SIZE 128
#define MMAPFS_INODES_PER_BLOCK (MMAPFS_BLOCK_SIZE / MMAPFS_INODE_SIZE)
#define MMAPFS_ROOT_INO 1
#define MMAPFS_BAD_INO 0

/* Block pointer constants */
#define MMAPFS_DIRECT_BLOCKS 12
#define MMAPFS_INDIRECT_PTRS (MMAPFS_BLOCK_SIZE / sizeof(uint32_t))

/* Directory entry constants */
#define MMAPFS_NAME_MAX 252
#define MMAPFS_DIRENT_SIZE 256
#define MMAPFS_DIRENTS_PER_BLOCK (MMAPFS_BLOCK_SIZE / MMAPFS_DIRENT_SIZE)

/* Journal constants */
#define MMAPFS_JOURNAL_BLOCKS 32
#define MMAPFS_JOURNAL_MAX_BLOCKS (MMAPFS_JOURNAL_BLOCKS - 1)

/* File descriptor constants */
#define MMAPFS_MAX_OPEN_FILES 1024

/* Default device size: 64MB */
#define MMAPFS_DEFAULT_SIZE (64UL * 1024 * 1024)

/* Default backing file path */
#define MMAPFS_DEFAULT_DEVICE "/tmp/mmapfs.img"

/* Environment variable names */
#define MMAPFS_ENV_DEVICE "MMAPFS_DEVICE"
#define MMAPFS_ENV_SIZE "MMAPFS_SIZE"

/* ============================================================
 * On-disk Structures
 * ============================================================ */

/* Superblock: stored in block 0, exactly one block in size */
struct mmapfs_super {
    uint32_t s_magic;             /* Magic number (MMAPFS_MAGIC) */
    uint32_t s_block_size;        /* Block size in bytes */
    uint32_t s_total_blocks;      /* Total number of blocks on device */
    uint32_t s_inode_count;       /* Total number of inodes */
    uint32_t s_free_blocks;       /* Number of free data blocks */
    uint32_t s_free_inodes;       /* Number of free inodes */
    uint32_t s_journal_start;     /* First journal block number */
    uint32_t s_journal_blocks;    /* Number of journal blocks */
    uint32_t s_inode_bitmap;      /* Inode bitmap block number */
    uint32_t s_data_bitmap;       /* Data bitmap start block number */
    uint32_t s_data_bitmap_blocks; /* Number of data bitmap blocks */
    uint32_t s_inode_table;       /* Inode table start block number */
    uint32_t s_inode_table_blocks; /* Number of inode table blocks */
    uint32_t s_data_start;        /* First data block number */
    uint32_t s_data_blocks;       /* Number of data blocks */
    uint32_t s_journal_seq;       /* Current journal sequence number */
    uint8_t s_padding[4096 - 64]; /* Pad to MMAPFS_BLOCK_SIZE */
};

/*
 * Inode: 128 bytes
 *
 * Layout:
 *   i_mode      (4)  + i_nlink  (4) + i_uid    (4) + i_gid     (4) = 16
 *   i_size      (8)  + i_atime  (8) + i_mtime  (8) + i_ctime   (8) = 32
 *   i_blocks    (4)  + i_flags  (4)                                 =  8
 *   i_direct[12](48) + i_indirect(4) + i_dindirect(4)              = 56
 *   i_padding[4](16)                                                = 16
 *   Total = 128
 */
struct mmapfs_inode {
    uint32_t i_mode;   /* File type and permissions (S_IFDIR, S_IFREG, ...) */
    uint32_t i_nlink;  /* Hard link count */
    uint32_t i_uid;    /* Owner user ID */
    uint32_t i_gid;    /* Owner group ID */
    uint64_t i_size;   /* File size in bytes */
    int64_t i_atime;   /* Access time (seconds since epoch) */
    int64_t i_mtime;   /* Modification time */
    int64_t i_ctime;   /* Status change time */
    uint32_t i_blocks; /* Number of allocated data blocks */
    uint32_t i_flags;  /* Inode flags (reserved) */
    uint32_t i_direct[MMAPFS_DIRECT_BLOCKS]; /* Direct block pointers */
    uint32_t i_indirect;   /* Single indirect block pointer */
    uint32_t i_dindirect;  /* Double indirect block pointer */
    uint32_t i_padding[4]; /* Pad to 128 bytes */
};

_Static_assert(sizeof(struct mmapfs_inode) == MMAPFS_INODE_SIZE,
               "mmapfs_inode must be exactly 128 bytes");

/* Directory entry: 256 bytes, fixed size */
struct mmapfs_dirent {
    uint32_t d_ino;                   /* Inode number (0 = unused/deleted) */
    char d_name[MMAPFS_NAME_MAX];     /* File name, null-terminated */
};

_Static_assert(sizeof(struct mmapfs_dirent) == MMAPFS_DIRENT_SIZE,
               "mmapfs_dirent must be exactly 256 bytes");

/*
 * Journal descriptor: stored in the first journal block.
 *
 * The journal uses redo logging:
 *   1. Write new block data to journal data blocks
 *   2. Flush journal data (msync)
 *   3. Set jd_committed = 1 and flush descriptor (atomic commit point)
 *   4. Copy journal data to real target blocks
 *   5. Flush real blocks
 *   6. Clear journal
 *
 * Recovery: if jd_committed == 1, replay journal data to targets.
 */
struct mmapfs_journal_desc {
    uint32_t jd_magic;      /* MMAPFS_JOURNAL_MAGIC */
    uint32_t jd_seq;        /* Transaction sequence number */
    uint32_t jd_num_blocks; /* Number of data blocks in transaction */
    uint32_t jd_committed;  /* 0 = uncommitted, 1 = committed */
    uint32_t jd_targets[MMAPFS_JOURNAL_MAX_BLOCKS]; /* Target block numbers */
    uint8_t jd_padding[MMAPFS_BLOCK_SIZE - 16 -
                        MMAPFS_JOURNAL_MAX_BLOCKS * 4];
};

_Static_assert(sizeof(struct mmapfs_journal_desc) == MMAPFS_BLOCK_SIZE,
               "mmapfs_journal_desc must be exactly one block");

/* ============================================================
 * In-memory Structures
 * ============================================================ */

/* Open file descriptor entry */
struct mmapfs_fd {
    int in_use;      /* Whether this FD slot is active */
    uint32_t ino;    /* Inode number */
    off_t offset;    /* Current read/write position */
    int flags;       /* Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.) */
};

/* Global filesystem state */
struct mmapfs_state {
    int dev_fd;                                    /* Backing file descriptor */
    void *dev_map;                                 /* mmap base address */
    size_t dev_size;                               /* Device/file size */
    struct mmapfs_super *super;                    /* Superblock pointer */
    struct mmapfs_fd fd_table[MMAPFS_MAX_OPEN_FILES]; /* File descriptor table */
    int initialized;                               /* Filesystem ready flag */
    pthread_mutex_t lock;                          /* Global filesystem lock */
};

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/* device.c: Device layer */
int mmapfs_device_init(const char *path, size_t size);
void mmapfs_device_close(void);
void *mmapfs_get_block(uint32_t block_no);
void mmapfs_sync_block(uint32_t block_no);
void mmapfs_sync_range(uint32_t start_block, uint32_t count);
void mmapfs_sync_all(void);

/* alloc.c: Block/inode allocation and formatting */
int mmapfs_format(void);
int mmapfs_bitmap_test(void *bitmap, uint32_t bit);
void mmapfs_bitmap_set(void *bitmap, uint32_t bit);
void mmapfs_bitmap_clear(void *bitmap, uint32_t bit);
uint32_t mmapfs_alloc_inode(void);
void mmapfs_free_inode(uint32_t ino);
uint32_t mmapfs_alloc_block(void);
void mmapfs_free_block(uint32_t block_no);

/* inode.c: Inode operations */
struct mmapfs_inode *mmapfs_inode_get(uint32_t ino);
int mmapfs_inode_init(uint32_t ino, mode_t mode);
uint32_t mmapfs_inode_get_block(struct mmapfs_inode *inode,
                                uint32_t file_block);
uint32_t mmapfs_inode_alloc_block(uint32_t ino, uint32_t file_block);
void mmapfs_inode_clear_block(struct mmapfs_inode *inode, uint32_t file_block);
int mmapfs_inode_truncate(uint32_t ino, off_t length);

/* dir.c: Directory operations */
const char *mmapfs_strip_prefix(const char *path);
uint32_t mmapfs_path_resolve(const char *path);
uint32_t mmapfs_path_resolve_parent(const char *path, char *name_out);
uint32_t mmapfs_dir_lookup(uint32_t dir_ino, const char *name);
int mmapfs_dir_add(uint32_t dir_ino, const char *name, uint32_t ino);
int mmapfs_dir_remove(uint32_t dir_ino, const char *name);
int mmapfs_dir_is_empty(uint32_t dir_ino);

/* journal.c: Write-ahead journal */
int mmapfs_journal_init(void);
int mmapfs_journal_begin(void);
int mmapfs_journal_write_block(uint32_t block_no, const void *data);
int mmapfs_journal_commit(void);
int mmapfs_journal_recover(void);

/* Global state accessor (defined in device.c) */
struct mmapfs_state *mmapfs_get_state(void);

#endif /* MMAPFS_H */
