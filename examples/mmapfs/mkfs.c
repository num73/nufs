/*
 * mkfs.c - Standalone format tool for mmapfs
 *
 * Creates a new mmapfs filesystem image on a file or block device.
 *
 * Usage:
 *   mkfs_mmapfs <device_path> [size_in_mb]
 *
 * If size_in_mb is omitted, defaults to 64 MB.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Minimal standalone formatter – duplicates just enough logic from
 * alloc.c / device.c so that we do not need to link the full library. */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device_path> [size_in_mb]\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    size_t size = MMAPFS_DEFAULT_SIZE;
    if (argc >= 3) {
        char *endptr;
        long mb = strtol(argv[2], &endptr, 10);
        if (*endptr != '\0' || mb <= 0) {
            fprintf(stderr, "Invalid size: %s\n", argv[2]);
            return 1;
        }
        size = (size_t)mb * 1024 * 1024;
    }

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (ftruncate(fd, (off_t)size) < 0) {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    void *map =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* Zero the image */
    memset(map, 0, size);

    uint32_t total_blocks = (uint32_t)(size / MMAPFS_BLOCK_SIZE);

    /* Compute layout */
    uint32_t journal_start = 1;
    uint32_t inode_bitmap = journal_start + MMAPFS_JOURNAL_BLOCKS;
    uint32_t data_bitmap = inode_bitmap + 1;
    uint32_t data_bitmap_blocks =
        (total_blocks + MMAPFS_BLOCK_SIZE * 8 - 1) / (MMAPFS_BLOCK_SIZE * 8);
    if (data_bitmap_blocks == 0)
        data_bitmap_blocks = 1;

    uint32_t inode_table = data_bitmap + data_bitmap_blocks;
    uint32_t inode_table_blocks = 256;
    if (inode_table_blocks > total_blocks / 4)
        inode_table_blocks = total_blocks / 4;
    if (inode_table_blocks == 0)
        inode_table_blocks = 1;

    uint32_t inode_count = inode_table_blocks * MMAPFS_INODES_PER_BLOCK;
    uint32_t data_start = inode_table + inode_table_blocks;
    uint32_t data_blocks =
        (total_blocks > data_start) ? total_blocks - data_start : 0;

    /* Write superblock */
    struct mmapfs_super *sb = (struct mmapfs_super *)map;
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

    /* Helper: get pointer to a block */
#define BLK(n) ((char *)map + (uint64_t)(n) * MMAPFS_BLOCK_SIZE)

    /* Reserve inodes 0 and 1 in bitmap */
    uint8_t *ibmap = (uint8_t *)BLK(inode_bitmap);
    ibmap[0] |= 0x03; /* bits 0 and 1 */
    sb->s_free_inodes -= 2;

    /* Initialize root inode (inode 1) */
    struct mmapfs_inode *root =
        (struct mmapfs_inode *)BLK(inode_table) + MMAPFS_ROOT_INO;
    root->i_mode = S_IFDIR | 0755;
    root->i_nlink = 2;
    root->i_uid = (uint32_t)getuid();
    root->i_gid = (uint32_t)getgid();
    root->i_atime = time(NULL);
    root->i_mtime = root->i_atime;
    root->i_ctime = root->i_atime;

    /* Allocate first data block for root directory */
    uint8_t *dbmap = (uint8_t *)BLK(data_bitmap);
    dbmap[0] |= 0x01; /* bit 0 = first data block */
    sb->s_free_blocks--;

    uint32_t root_block = data_start;
    root->i_direct[0] = root_block;
    root->i_blocks = 1;
    root->i_size = 2 * MMAPFS_DIRENT_SIZE;

    /* "." and ".." entries */
    struct mmapfs_dirent *de = (struct mmapfs_dirent *)BLK(root_block);
    de[0].d_ino = MMAPFS_ROOT_INO;
    strncpy(de[0].d_name, ".", MMAPFS_NAME_MAX - 1);
    de[1].d_ino = MMAPFS_ROOT_INO;
    strncpy(de[1].d_name, "..", MMAPFS_NAME_MAX - 1);

#undef BLK

    /* Persist */
    msync(map, size, MS_SYNC);
    munmap(map, size);
    close(fd);

    printf("mmapfs: formatted %s\n", path);
    printf("  Total size : %zu MB (%u blocks)\n", size / (1024 * 1024),
           total_blocks);
    printf("  Data blocks: %u (%.1f MB)\n", data_blocks,
           (double)data_blocks * MMAPFS_BLOCK_SIZE / (1024 * 1024));
    printf("  Inodes     : %u\n", inode_count);
    printf("  Journal    : %u blocks\n", MMAPFS_JOURNAL_BLOCKS);

    return 0;
}
