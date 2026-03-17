/*
 * inode.c - Inode operations for mmapfs
 *
 * Provides inode lookup, initialization, block mapping (direct,
 * single-indirect, and double-indirect), and truncation.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Return a pointer to the on-disk inode for the given inode number.
 * The pointer is directly into the mmap'd region. */
struct mmapfs_inode *mmapfs_inode_get(uint32_t ino) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    uint32_t block_idx = ino / MMAPFS_INODES_PER_BLOCK;
    uint32_t block_off = ino % MMAPFS_INODES_PER_BLOCK;

    void *block = mmapfs_get_block(sb->s_inode_table + block_idx);
    return (struct mmapfs_inode *)block + block_off;
}

/* Initialize a freshly allocated inode with the given mode. */
int mmapfs_inode_init(uint32_t ino, mode_t mode) {
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    memset(inode, 0, sizeof(*inode));

    inode->i_mode = mode;
    inode->i_nlink = 1;
    inode->i_uid = (uint32_t)getuid();
    inode->i_gid = (uint32_t)getgid();
    inode->i_atime = time(NULL);
    inode->i_mtime = inode->i_atime;
    inode->i_ctime = inode->i_atime;

    return 0;
}

/* Translate a file-relative block index to an absolute disk block number.
 * Returns 0 if the block is not allocated (sparse hole). */
uint32_t mmapfs_inode_get_block(struct mmapfs_inode *inode,
                                uint32_t file_block) {
    /* Direct blocks: 0 .. 11 */
    if (file_block < MMAPFS_DIRECT_BLOCKS)
        return inode->i_direct[file_block];

    file_block -= MMAPFS_DIRECT_BLOCKS;

    /* Single indirect: 12 .. 12+1023 */
    if (file_block < MMAPFS_INDIRECT_PTRS) {
        if (inode->i_indirect == 0)
            return 0;
        uint32_t *indirect =
            (uint32_t *)mmapfs_get_block(inode->i_indirect);
        return indirect[file_block];
    }

    file_block -= MMAPFS_INDIRECT_PTRS;

    /* Double indirect: 12+1024 .. 12+1024+1024*1024-1 */
    if (file_block < MMAPFS_INDIRECT_PTRS * MMAPFS_INDIRECT_PTRS) {
        if (inode->i_dindirect == 0)
            return 0;
        uint32_t *dindirect =
            (uint32_t *)mmapfs_get_block(inode->i_dindirect);
        uint32_t idx1 = file_block / MMAPFS_INDIRECT_PTRS;
        uint32_t idx2 = file_block % MMAPFS_INDIRECT_PTRS;
        if (dindirect[idx1] == 0)
            return 0;
        uint32_t *indirect =
            (uint32_t *)mmapfs_get_block(dindirect[idx1]);
        return indirect[idx2];
    }

    return 0; /* File offset too large */
}

/* Allocate a new data block for the given file block index.
 * Allocates any necessary indirect blocks as well.
 * Returns the disk block number on success, 0 on failure. */
uint32_t mmapfs_inode_alloc_block(uint32_t ino, uint32_t file_block) {
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);

    uint32_t new_block = mmapfs_alloc_block();
    if (new_block == 0)
        return 0;

    /* Direct blocks */
    if (file_block < MMAPFS_DIRECT_BLOCKS) {
        inode->i_direct[file_block] = new_block;
        inode->i_blocks++;
        return new_block;
    }

    uint32_t fb = file_block - MMAPFS_DIRECT_BLOCKS;

    /* Single indirect */
    if (fb < MMAPFS_INDIRECT_PTRS) {
        if (inode->i_indirect == 0) {
            uint32_t ind = mmapfs_alloc_block();
            if (ind == 0) {
                mmapfs_free_block(new_block);
                return 0;
            }
            inode->i_indirect = ind;
            inode->i_blocks++;
        }
        uint32_t *indirect =
            (uint32_t *)mmapfs_get_block(inode->i_indirect);
        indirect[fb] = new_block;
        inode->i_blocks++;
        return new_block;
    }

    fb -= MMAPFS_INDIRECT_PTRS;

    /* Double indirect */
    if (inode->i_dindirect == 0) {
        uint32_t dind = mmapfs_alloc_block();
        if (dind == 0) {
            mmapfs_free_block(new_block);
            return 0;
        }
        inode->i_dindirect = dind;
        inode->i_blocks++;
    }

    uint32_t *dindirect =
        (uint32_t *)mmapfs_get_block(inode->i_dindirect);
    uint32_t idx1 = fb / MMAPFS_INDIRECT_PTRS;
    uint32_t idx2 = fb % MMAPFS_INDIRECT_PTRS;

    if (dindirect[idx1] == 0) {
        uint32_t ind = mmapfs_alloc_block();
        if (ind == 0) {
            mmapfs_free_block(new_block);
            return 0;
        }
        dindirect[idx1] = ind;
        inode->i_blocks++;
    }

    uint32_t *indirect =
        (uint32_t *)mmapfs_get_block(dindirect[idx1]);
    indirect[idx2] = new_block;
    inode->i_blocks++;
    return new_block;
}

/* Clear a block pointer for the given file block index. */
void mmapfs_inode_clear_block(struct mmapfs_inode *inode,
                              uint32_t file_block) {
    if (file_block < MMAPFS_DIRECT_BLOCKS) {
        inode->i_direct[file_block] = 0;
        return;
    }

    uint32_t fb = file_block - MMAPFS_DIRECT_BLOCKS;

    if (fb < MMAPFS_INDIRECT_PTRS) {
        if (inode->i_indirect == 0)
            return;
        uint32_t *indirect =
            (uint32_t *)mmapfs_get_block(inode->i_indirect);
        indirect[fb] = 0;
        return;
    }

    fb -= MMAPFS_INDIRECT_PTRS;

    if (inode->i_dindirect == 0)
        return;
    uint32_t *dindirect =
        (uint32_t *)mmapfs_get_block(inode->i_dindirect);
    uint32_t idx1 = fb / MMAPFS_INDIRECT_PTRS;
    uint32_t idx2 = fb % MMAPFS_INDIRECT_PTRS;

    if (dindirect[idx1] == 0)
        return;
    uint32_t *indirect =
        (uint32_t *)mmapfs_get_block(dindirect[idx1]);
    indirect[idx2] = 0;
}

/* Truncate a file to the given length.
 * Frees blocks beyond the new size and zeroes any partial tail block. */
int mmapfs_inode_truncate(uint32_t ino, off_t length) {
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);

    if ((uint64_t)length >= inode->i_size) {
        /* Extending (or no change): just update size */
        inode->i_size = (uint64_t)length;
        inode->i_mtime = time(NULL);
        inode->i_ctime = inode->i_mtime;
        return 0;
    }

    /* Shrinking: free blocks beyond new end */
    uint32_t new_nblocks =
        ((uint64_t)length + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;
    uint32_t old_nblocks =
        (inode->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    for (uint32_t i = new_nblocks; i < old_nblocks; i++) {
        uint32_t blk = mmapfs_inode_get_block(inode, i);
        if (blk != 0) {
            mmapfs_free_block(blk);
            mmapfs_inode_clear_block(inode, i);
            inode->i_blocks--;
        }
    }

    /* Free indirect metadata blocks that are now completely empty.
     * Only the single-indirect block is checked here for simplicity;
     * a production filesystem would also clean up double-indirect
     * entries. */
    if (new_nblocks <= MMAPFS_DIRECT_BLOCKS && inode->i_indirect != 0) {
        mmapfs_free_block(inode->i_indirect);
        inode->i_indirect = 0;
        inode->i_blocks--;
    }

    /* Zero the partial tail of the last remaining block */
    if (length > 0 && (uint64_t)length % MMAPFS_BLOCK_SIZE != 0) {
        uint32_t last_blk =
            mmapfs_inode_get_block(inode, new_nblocks - 1);
        if (last_blk != 0) {
            char *data = (char *)mmapfs_get_block(last_blk);
            size_t off = (uint64_t)length % MMAPFS_BLOCK_SIZE;
            memset(data + off, 0, MMAPFS_BLOCK_SIZE - off);
        }
    }

    inode->i_size = (uint64_t)length;
    inode->i_mtime = time(NULL);
    inode->i_ctime = inode->i_mtime;
    return 0;
}
