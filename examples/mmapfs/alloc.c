/*
 * alloc.c - Block/inode allocation and filesystem formatting for mmapfs
 *
 * Manages free space using bitmaps:
 *   - Inode bitmap: one bit per inode (1 = allocated, 0 = free)
 *   - Data bitmap:  one bit per data block
 *
 * Also contains the filesystem format routine that initializes all
 * on-disk structures including the root directory.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <string.h>
#include <time.h>

/* ---- Bitmap helpers ---- */

int mmapfs_bitmap_test(void *bitmap, uint32_t bit) {
    uint8_t *bytes = (uint8_t *)bitmap;
    return (bytes[bit / 8] >> (bit % 8)) & 1;
}

void mmapfs_bitmap_set(void *bitmap, uint32_t bit) {
    uint8_t *bytes = (uint8_t *)bitmap;
    bytes[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

void mmapfs_bitmap_clear(void *bitmap, uint32_t bit) {
    uint8_t *bytes = (uint8_t *)bitmap;
    bytes[bit / 8] &= (uint8_t) ~(1 << (bit % 8));
}

/* ---- Inode allocation ---- */

uint32_t mmapfs_alloc_inode(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    if (sb->s_free_inodes == 0)
        return 0;

    void *ibmap = mmapfs_get_block(sb->s_inode_bitmap);

    for (uint32_t i = MMAPFS_ROOT_INO; i < sb->s_inode_count; i++) {
        if (!mmapfs_bitmap_test(ibmap, i)) {
            mmapfs_bitmap_set(ibmap, i);
            sb->s_free_inodes--;
            return i;
        }
    }
    return 0;
}

void mmapfs_free_inode(uint32_t ino) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    void *ibmap = mmapfs_get_block(sb->s_inode_bitmap);
    mmapfs_bitmap_clear(ibmap, ino);
    sb->s_free_inodes++;

    /* Clear the inode on disk */
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    memset(inode, 0, sizeof(*inode));
}

/* ---- Data block allocation ---- */

uint32_t mmapfs_alloc_block(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    if (sb->s_free_blocks == 0)
        return 0;

    for (uint32_t b = 0; b < sb->s_data_bitmap_blocks; b++) {
        void *dbmap = mmapfs_get_block(sb->s_data_bitmap + b);
        uint32_t base = b * MMAPFS_BLOCK_SIZE * 8;

        for (uint32_t i = 0; i < MMAPFS_BLOCK_SIZE * 8; i++) {
            uint32_t idx = base + i;
            if (idx >= sb->s_data_blocks)
                return 0;
            if (!mmapfs_bitmap_test(dbmap, i)) {
                mmapfs_bitmap_set(dbmap, i);
                sb->s_free_blocks--;
                uint32_t block_no = sb->s_data_start + idx;
                /* Zero the newly allocated block */
                memset(mmapfs_get_block(block_no), 0, MMAPFS_BLOCK_SIZE);
                return block_no;
            }
        }
    }
    return 0;
}

void mmapfs_free_block(uint32_t block_no) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    if (block_no < sb->s_data_start)
        return;

    uint32_t idx = block_no - sb->s_data_start;
    uint32_t bmap_block = idx / (MMAPFS_BLOCK_SIZE * 8);
    uint32_t bmap_bit = idx % (MMAPFS_BLOCK_SIZE * 8);

    void *dbmap = mmapfs_get_block(sb->s_data_bitmap + bmap_block);
    mmapfs_bitmap_clear(dbmap, bmap_bit);
    sb->s_free_blocks++;
}

/* ---- Filesystem formatting ---- */

int mmapfs_format(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    uint32_t total_blocks = (uint32_t)(st->dev_size / MMAPFS_BLOCK_SIZE);

    /* Zero the entire device */
    memset(st->dev_map, 0, st->dev_size);

    /* Compute layout */
    uint32_t journal_start = 1;
    uint32_t inode_bitmap = journal_start + MMAPFS_JOURNAL_BLOCKS; /* 33 */
    uint32_t data_bitmap = inode_bitmap + 1;                       /* 34 */

    /* Data bitmap: one block per 32768 data blocks (128 MB) */
    uint32_t data_bitmap_blocks =
        (total_blocks + MMAPFS_BLOCK_SIZE * 8 - 1) / (MMAPFS_BLOCK_SIZE * 8);
    if (data_bitmap_blocks == 0)
        data_bitmap_blocks = 1;

    uint32_t inode_table = data_bitmap + data_bitmap_blocks;

    /* Inode count: limited by bitmap (1 block = 32768 inodes) and
     * a reasonable number of inode table blocks (256 blocks = 8192 inodes) */
    uint32_t inode_table_blocks = 256;
    if (inode_table_blocks > total_blocks / 4)
        inode_table_blocks = total_blocks / 4;
    if (inode_table_blocks == 0)
        inode_table_blocks = 1;

    uint32_t inode_count = inode_table_blocks * MMAPFS_INODES_PER_BLOCK;
    uint32_t data_start = inode_table + inode_table_blocks;
    uint32_t data_blocks = 0;
    if (total_blocks > data_start)
        data_blocks = total_blocks - data_start;

    /* Fill in superblock */
    struct mmapfs_super *sb = st->super;
    sb->s_magic = MMAPFS_MAGIC;
    sb->s_block_size = MMAPFS_BLOCK_SIZE;
    sb->s_total_blocks = total_blocks;
    sb->s_inode_count = inode_count;
    sb->s_free_blocks = data_blocks;
    sb->s_free_inodes = inode_count;
    sb->s_journal_start = journal_start;
    sb->s_journal_blocks = MMAPFS_JOURNAL_BLOCKS;
    sb->s_inode_bitmap = inode_bitmap;
    sb->s_data_bitmap = data_bitmap;
    sb->s_data_bitmap_blocks = data_bitmap_blocks;
    sb->s_inode_table = inode_table;
    sb->s_inode_table_blocks = inode_table_blocks;
    sb->s_data_start = data_start;
    sb->s_data_blocks = data_blocks;
    sb->s_journal_seq = 0;

    /* Reserve inode 0 (bad inode) and inode 1 (root) in bitmap */
    void *ibmap = mmapfs_get_block(inode_bitmap);
    mmapfs_bitmap_set(ibmap, MMAPFS_BAD_INO);
    mmapfs_bitmap_set(ibmap, MMAPFS_ROOT_INO);
    sb->s_free_inodes -= 2;

    /* Initialize root inode */
    struct mmapfs_inode *root = mmapfs_inode_get(MMAPFS_ROOT_INO);
    root->i_mode = S_IFDIR | 0755;
    root->i_nlink = 2; /* . and parent (root's parent is itself) */
    root->i_uid = 0;
    root->i_gid = 0;
    root->i_size = 0;
    root->i_atime = time(NULL);
    root->i_mtime = root->i_atime;
    root->i_ctime = root->i_atime;
    root->i_blocks = 0;

    /* Allocate one data block for the root directory and add . and .. */
    uint32_t root_block = mmapfs_alloc_block();
    if (root_block == 0)
        return -ENOSPC;

    root->i_direct[0] = root_block;
    root->i_blocks = 1;
    root->i_size = 2 * MMAPFS_DIRENT_SIZE;

    struct mmapfs_dirent *dentries =
        (struct mmapfs_dirent *)mmapfs_get_block(root_block);

    /* "." entry */
    dentries[0].d_ino = MMAPFS_ROOT_INO;
    strncpy(dentries[0].d_name, ".", MMAPFS_NAME_MAX - 1);

    /* ".." entry (root's parent is itself) */
    dentries[1].d_ino = MMAPFS_ROOT_INO;
    strncpy(dentries[1].d_name, "..", MMAPFS_NAME_MAX - 1);

    /* Persist everything */
    mmapfs_sync_all();

    return 0;
}
