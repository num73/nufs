/*
 * device.c - Device layer for mmapfs
 *
 * Provides mmap-based access to a raw file or block device.
 * All filesystem data is accessed through the memory-mapped region.
 *
 * The backing file path is determined by the MMAPFS_DEVICE environment
 * variable (default: /tmp/mmapfs.img). The size is determined by
 * MMAPFS_SIZE (default: 64MB).
 *
 * Safety note: All internal file operations (open, mmap, msync, close)
 * use paths outside /mnt/nufs and regular file descriptors (without
 * NUFS_FD_PREFIX), so they pass through the syscall interception layer
 * to the real kernel syscalls without circular interception.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Single global filesystem state instance */
static struct mmapfs_state g_state;

struct mmapfs_state *mmapfs_get_state(void) { return &g_state; }

int mmapfs_device_init(const char *path, size_t size) {
    struct stat st;
    int fd;
    int need_format = 0;

    memset(&g_state, 0, sizeof(g_state));
    g_state.dev_fd = -1;
    pthread_mutex_init(&g_state.lock, NULL);

    /* Open (or create) the backing file */
    fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
        return -errno;

    /* Check current file size */
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -errno;
    }

    /* Extend the file if it is smaller than the requested size */
    if ((size_t)st.st_size < size) {
        if (ftruncate(fd, (off_t)size) < 0) {
            close(fd);
            return -errno;
        }
        need_format = 1;
    }

    /* Memory-map the entire device */
    g_state.dev_map =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_state.dev_map == MAP_FAILED) {
        g_state.dev_map = NULL;
        close(fd);
        return -errno;
    }

    g_state.dev_fd = fd;
    g_state.dev_size = size;
    g_state.super = (struct mmapfs_super *)g_state.dev_map;

    /* Determine whether we need to format or just recover */
    if (!need_format && g_state.super->s_magic == MMAPFS_MAGIC) {
        mmapfs_journal_recover();
    } else {
        mmapfs_format();
    }

    g_state.initialized = 1;
    return 0;
}

void mmapfs_device_close(void) {
    if (g_state.dev_map) {
        msync(g_state.dev_map, g_state.dev_size, MS_SYNC);
        munmap(g_state.dev_map, g_state.dev_size);
        g_state.dev_map = NULL;
    }
    if (g_state.dev_fd >= 0) {
        close(g_state.dev_fd);
        g_state.dev_fd = -1;
    }
    g_state.initialized = 0;
    pthread_mutex_destroy(&g_state.lock);
}

void *mmapfs_get_block(uint32_t block_no) {
    return (char *)g_state.dev_map +
           (uint64_t)block_no * MMAPFS_BLOCK_SIZE;
}

void mmapfs_sync_block(uint32_t block_no) {
    void *addr = mmapfs_get_block(block_no);
    msync(addr, MMAPFS_BLOCK_SIZE, MS_SYNC);
}

void mmapfs_sync_range(uint32_t start_block, uint32_t count) {
    void *addr = mmapfs_get_block(start_block);
    msync(addr, (size_t)count * MMAPFS_BLOCK_SIZE, MS_SYNC);
}

void mmapfs_sync_all(void) {
    msync(g_state.dev_map, g_state.dev_size, MS_SYNC);
}
