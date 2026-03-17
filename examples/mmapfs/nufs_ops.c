/*
 * nufs_ops.c - Implementation of all nufs_* POSIX API functions
 *
 * This file connects the mmapfs internal subsystems (device, alloc,
 * inode, dir, journal) to the public nufs_* interface declared in
 * src/nufs.h.  It also manages the open-file descriptor table and
 * provides the nufs_init() entry point.
 *
 * Threading: a single global mutex serialises all filesystem operations.
 * This is simple and correct, though a finer-grained locking scheme
 * would be needed for production workloads.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include "nufs.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---- Helpers ---- */

#define MMAPFS_LOCK() pthread_mutex_lock(&mmapfs_get_state()->lock)
#define MMAPFS_UNLOCK() pthread_mutex_unlock(&mmapfs_get_state()->lock)
#define MMAPFS_FD_TO_INDEX(fd) ((fd) - NUFS_FD_PREFIX)

/* Validate an FD and return the table entry, or NULL on error. */
static struct mmapfs_fd *fd_get(int fd) {
    int idx = MMAPFS_FD_TO_INDEX(fd);
    struct mmapfs_state *st = mmapfs_get_state();
    if (idx < 0 || idx >= MMAPFS_MAX_OPEN_FILES)
        return NULL;
    if (!st->fd_table[idx].in_use)
        return NULL;
    return &st->fd_table[idx];
}

/* Allocate a new FD slot and return its full fd value, or -EMFILE. */
static int fd_alloc(uint32_t ino, int flags) {
    struct mmapfs_state *st = mmapfs_get_state();
    for (int i = 0; i < MMAPFS_MAX_OPEN_FILES; i++) {
        if (!st->fd_table[i].in_use) {
            st->fd_table[i].in_use = 1;
            st->fd_table[i].ino = ino;
            st->fd_table[i].offset = 0;
            st->fd_table[i].flags = flags;
            return NUFS_FD_PREFIX + i;
        }
    }
    return -EMFILE;
}

/* Fill a struct stat from an inode. */
static void fill_stat(struct stat *st, uint32_t ino,
                       struct mmapfs_inode *inode) {
    memset(st, 0, sizeof(*st));
    st->st_ino = ino;
    st->st_mode = inode->i_mode;
    st->st_nlink = inode->i_nlink;
    st->st_uid = inode->i_uid;
    st->st_gid = inode->i_gid;
    st->st_size = (off_t)inode->i_size;
    st->st_blocks = (blkcnt_t)inode->i_blocks *
                    (MMAPFS_BLOCK_SIZE / 512);
    st->st_blksize = MMAPFS_BLOCK_SIZE;
    st->st_atime = (time_t)inode->i_atime;
    st->st_mtime = (time_t)inode->i_mtime;
    st->st_ctime = (time_t)inode->i_ctime;
}

/* ---- nufs_init ---- */

void nufs_init(void) {
    const char *dev = getenv(MMAPFS_ENV_DEVICE);
    if (!dev)
        dev = MMAPFS_DEFAULT_DEVICE;

    size_t size = MMAPFS_DEFAULT_SIZE;
    const char *env_size = getenv(MMAPFS_ENV_SIZE);
    if (env_size) {
        size_t mb = (size_t)atol(env_size);
        if (mb > 0)
            size = mb * 1024 * 1024;
    }

    mmapfs_device_init(dev, size);
}

/* ---- File open / create / close ---- */

int nufs_open(const char *pathname, int flags, mode_t mode) {
    MMAPFS_LOCK();
    uint32_t ino = mmapfs_path_resolve(pathname);

    if (ino == 0) {
        if (!(flags & O_CREAT)) {
            MMAPFS_UNLOCK();
            return -ENOENT;
        }
        /* Create the file */
        char name[MMAPFS_NAME_MAX];
        uint32_t parent = mmapfs_path_resolve_parent(pathname, name);
        if (parent == 0) {
            MMAPFS_UNLOCK();
            return -ENOENT;
        }
        struct mmapfs_inode *pnode = mmapfs_inode_get(parent);
        if (!S_ISDIR(pnode->i_mode)) {
            MMAPFS_UNLOCK();
            return -ENOTDIR;
        }

        uint32_t new_ino = mmapfs_alloc_inode();
        if (new_ino == 0) {
            MMAPFS_UNLOCK();
            return -ENOSPC;
        }
        mmapfs_inode_init(new_ino, S_IFREG | (mode & 0777));

        int rc = mmapfs_dir_add(parent, name, new_ino);
        if (rc < 0) {
            mmapfs_free_inode(new_ino);
            MMAPFS_UNLOCK();
            return rc;
        }
        ino = new_ino;
    } else {
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            MMAPFS_UNLOCK();
            return -EEXIST;
        }
        if (flags & O_TRUNC)
            mmapfs_inode_truncate(ino, 0);
    }

    int fd = fd_alloc(ino, flags);
    MMAPFS_UNLOCK();
    return fd;
}

int nufs_creat(const char *pathname, mode_t mode) {
    return nufs_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int nufs_close(int fd) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }
    fde->in_use = 0;
    MMAPFS_UNLOCK();
    return 0;
}

/* ---- Read / Write ---- */

ssize_t nufs_read(int fd, void *buf, size_t count) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    if (fde->offset >= (off_t)inode->i_size) {
        MMAPFS_UNLOCK();
        return 0;
    }
    if ((uint64_t)fde->offset + count > inode->i_size)
        count = inode->i_size - (uint64_t)fde->offset;

    ssize_t total = 0;
    while (count > 0) {
        uint32_t fb = (uint64_t)fde->offset / MMAPFS_BLOCK_SIZE;
        uint32_t boff = (uint64_t)fde->offset % MMAPFS_BLOCK_SIZE;
        uint32_t chunk = MMAPFS_BLOCK_SIZE - boff;
        if (chunk > count)
            chunk = (uint32_t)count;

        uint32_t blk = mmapfs_inode_get_block(inode, fb);
        if (blk == 0)
            memset(buf, 0, chunk); /* sparse hole */
        else
            memcpy(buf, (char *)mmapfs_get_block(blk) + boff, chunk);

        buf = (char *)buf + chunk;
        fde->offset += chunk;
        count -= chunk;
        total += chunk;
    }
    inode->i_atime = time(NULL);
    MMAPFS_UNLOCK();
    return total;
}

ssize_t nufs_pread(int fd, void *buf, size_t count, off_t offset) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    if (offset >= (off_t)inode->i_size) {
        MMAPFS_UNLOCK();
        return 0;
    }
    if ((uint64_t)offset + count > inode->i_size)
        count = inode->i_size - (uint64_t)offset;

    ssize_t total = 0;
    while (count > 0) {
        uint32_t fb = (uint64_t)offset / MMAPFS_BLOCK_SIZE;
        uint32_t boff = (uint64_t)offset % MMAPFS_BLOCK_SIZE;
        uint32_t chunk = MMAPFS_BLOCK_SIZE - boff;
        if (chunk > count)
            chunk = (uint32_t)count;

        uint32_t blk = mmapfs_inode_get_block(inode, fb);
        if (blk == 0)
            memset(buf, 0, chunk);
        else
            memcpy(buf, (char *)mmapfs_get_block(blk) + boff, chunk);

        buf = (char *)buf + chunk;
        offset += chunk;
        count -= chunk;
        total += chunk;
    }
    inode->i_atime = time(NULL);
    MMAPFS_UNLOCK();
    return total;
}

ssize_t nufs_write(int fd, const void *buf, size_t count) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    if (fde->flags & O_APPEND)
        fde->offset = (off_t)inode->i_size;

    ssize_t total = 0;
    while (count > 0) {
        uint32_t fb = (uint64_t)fde->offset / MMAPFS_BLOCK_SIZE;
        uint32_t boff = (uint64_t)fde->offset % MMAPFS_BLOCK_SIZE;
        uint32_t chunk = MMAPFS_BLOCK_SIZE - boff;
        if (chunk > count)
            chunk = (uint32_t)count;

        uint32_t blk = mmapfs_inode_get_block(inode, fb);
        if (blk == 0) {
            blk = mmapfs_inode_alloc_block(fde->ino, fb);
            if (blk == 0) {
                if (total > 0)
                    break;
                MMAPFS_UNLOCK();
                return -ENOSPC;
            }
            inode = mmapfs_inode_get(fde->ino);
        }
        memcpy((char *)mmapfs_get_block(blk) + boff, buf, chunk);

        buf = (const char *)buf + chunk;
        fde->offset += chunk;
        count -= chunk;
        total += chunk;
    }

    if ((uint64_t)fde->offset > inode->i_size)
        inode->i_size = (uint64_t)fde->offset;
    inode->i_mtime = time(NULL);
    inode->i_ctime = inode->i_mtime;
    MMAPFS_UNLOCK();
    return total;
}

ssize_t nufs_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    ssize_t total = 0;

    while (count > 0) {
        uint32_t fb = (uint64_t)offset / MMAPFS_BLOCK_SIZE;
        uint32_t boff = (uint64_t)offset % MMAPFS_BLOCK_SIZE;
        uint32_t chunk = MMAPFS_BLOCK_SIZE - boff;
        if (chunk > count)
            chunk = (uint32_t)count;

        uint32_t blk = mmapfs_inode_get_block(inode, fb);
        if (blk == 0) {
            blk = mmapfs_inode_alloc_block(fde->ino, fb);
            if (blk == 0) {
                if (total > 0)
                    break;
                MMAPFS_UNLOCK();
                return -ENOSPC;
            }
            inode = mmapfs_inode_get(fde->ino);
        }
        memcpy((char *)mmapfs_get_block(blk) + boff, buf, chunk);

        buf = (const char *)buf + chunk;
        offset += chunk;
        count -= chunk;
        total += chunk;
    }

    if ((uint64_t)offset > inode->i_size)
        inode->i_size = (uint64_t)offset;
    inode->i_mtime = time(NULL);
    inode->i_ctime = inode->i_mtime;
    MMAPFS_UNLOCK();
    return total;
}

/* ---- Seek ---- */

off_t nufs_lseek(int fd, off_t offset, int whence) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    off_t new_off;

    switch (whence) {
    case SEEK_SET:
        new_off = offset;
        break;
    case SEEK_CUR:
        new_off = fde->offset + offset;
        break;
    case SEEK_END:
        new_off = (off_t)inode->i_size + offset;
        break;
    default:
        MMAPFS_UNLOCK();
        return -EINVAL;
    }

    if (new_off < 0) {
        MMAPFS_UNLOCK();
        return -EINVAL;
    }

    fde->offset = new_off;
    MMAPFS_UNLOCK();
    return new_off;
}

/* ---- Directory operations ---- */

int nufs_mkdir(const char *pathname, mode_t mode) {
    MMAPFS_LOCK();

    char name[MMAPFS_NAME_MAX];
    uint32_t parent = mmapfs_path_resolve_parent(pathname, name);
    if (parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    struct mmapfs_inode *pnode = mmapfs_inode_get(parent);
    if (!S_ISDIR(pnode->i_mode)) {
        MMAPFS_UNLOCK();
        return -ENOTDIR;
    }

    if (mmapfs_dir_lookup(parent, name) != 0) {
        MMAPFS_UNLOCK();
        return -EEXIST;
    }

    uint32_t ino = mmapfs_alloc_inode();
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOSPC;
    }

    /* Use the journal for atomic multi-block metadata update */
    mmapfs_journal_begin();

    mmapfs_inode_init(ino, S_IFDIR | (mode & 0777));
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    inode->i_nlink = 2; /* . and parent */

    /* Allocate a data block for the new directory */
    uint32_t blk = mmapfs_alloc_block();
    if (blk == 0) {
        mmapfs_free_inode(ino);
        MMAPFS_UNLOCK();
        return -ENOSPC;
    }

    inode->i_direct[0] = blk;
    inode->i_blocks = 1;
    inode->i_size = 2 * MMAPFS_DIRENT_SIZE;

    /* Write "." and ".." */
    struct mmapfs_dirent *de =
        (struct mmapfs_dirent *)mmapfs_get_block(blk);
    de[0].d_ino = ino;
    memset(de[0].d_name, 0, MMAPFS_NAME_MAX);
    strncpy(de[0].d_name, ".", MMAPFS_NAME_MAX - 1);
    de[1].d_ino = parent;
    memset(de[1].d_name, 0, MMAPFS_NAME_MAX);
    strncpy(de[1].d_name, "..", MMAPFS_NAME_MAX - 1);

    /* Add entry in parent */
    int rc = mmapfs_dir_add(parent, name, ino);
    if (rc < 0) {
        mmapfs_free_block(blk);
        mmapfs_free_inode(ino);
        MMAPFS_UNLOCK();
        return rc;
    }

    /* Bump parent nlink (for the ".." reference) */
    pnode = mmapfs_inode_get(parent);
    pnode->i_nlink++;
    pnode->i_mtime = time(NULL);
    pnode->i_ctime = pnode->i_mtime;

    /* Journal: record the modified metadata blocks */
    struct mmapfs_state *st = mmapfs_get_state();
    mmapfs_journal_write_block(
        st->super->s_inode_bitmap,
        mmapfs_get_block(st->super->s_inode_bitmap));
    mmapfs_journal_write_block(
        blk, mmapfs_get_block(blk));

    mmapfs_journal_commit();
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_rmdir(const char *pathname) {
    MMAPFS_LOCK();

    char name[MMAPFS_NAME_MAX];
    uint32_t parent = mmapfs_path_resolve_parent(pathname, name);
    if (parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    uint32_t ino = mmapfs_dir_lookup(parent, name);
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    if (!S_ISDIR(inode->i_mode)) {
        MMAPFS_UNLOCK();
        return -ENOTDIR;
    }
    if (!mmapfs_dir_is_empty(ino)) {
        MMAPFS_UNLOCK();
        return -ENOTEMPTY;
    }

    mmapfs_journal_begin();

    /* Free directory data blocks */
    mmapfs_inode_truncate(ino, 0);
    mmapfs_dir_remove(parent, name);
    mmapfs_free_inode(ino);

    struct mmapfs_inode *pnode = mmapfs_inode_get(parent);
    if (pnode->i_nlink > 0)
        pnode->i_nlink--;
    pnode->i_mtime = time(NULL);
    pnode->i_ctime = pnode->i_mtime;

    struct mmapfs_state *st = mmapfs_get_state();
    mmapfs_journal_write_block(
        st->super->s_inode_bitmap,
        mmapfs_get_block(st->super->s_inode_bitmap));

    mmapfs_journal_commit();
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_rename(const char *oldname, const char *newname) {
    MMAPFS_LOCK();

    char old_base[MMAPFS_NAME_MAX];
    uint32_t old_parent = mmapfs_path_resolve_parent(oldname, old_base);
    if (old_parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    uint32_t ino = mmapfs_dir_lookup(old_parent, old_base);
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    char new_base[MMAPFS_NAME_MAX];
    uint32_t new_parent = mmapfs_path_resolve_parent(newname, new_base);
    if (new_parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    /* If target already exists, remove it first */
    uint32_t existing = mmapfs_dir_lookup(new_parent, new_base);
    if (existing != 0) {
        struct mmapfs_inode *ei = mmapfs_inode_get(existing);
        if (S_ISDIR(ei->i_mode)) {
            if (!mmapfs_dir_is_empty(existing)) {
                MMAPFS_UNLOCK();
                return -ENOTEMPTY;
            }
            mmapfs_inode_truncate(existing, 0);
        } else {
            mmapfs_inode_truncate(existing, 0);
        }
        mmapfs_dir_remove(new_parent, new_base);
        mmapfs_free_inode(existing);
    }

    mmapfs_journal_begin();

    mmapfs_dir_remove(old_parent, old_base);
    mmapfs_dir_add(new_parent, new_base, ino);

    /* Update ".." if renaming a directory across parents */
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    if (S_ISDIR(inode->i_mode) && old_parent != new_parent) {
        uint32_t nblocks =
            (inode->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;
        for (uint32_t b = 0; b < nblocks; b++) {
            uint32_t blk = mmapfs_inode_get_block(inode, b);
            if (blk == 0)
                continue;
            struct mmapfs_dirent *de =
                (struct mmapfs_dirent *)mmapfs_get_block(blk);
            for (uint32_t i = 0; i < MMAPFS_DIRENTS_PER_BLOCK; i++) {
                if (de[i].d_ino != 0 &&
                    strcmp(de[i].d_name, "..") == 0) {
                    de[i].d_ino = new_parent;
                    break;
                }
            }
        }
        struct mmapfs_inode *op = mmapfs_inode_get(old_parent);
        if (op->i_nlink > 0)
            op->i_nlink--;
        struct mmapfs_inode *np = mmapfs_inode_get(new_parent);
        np->i_nlink++;
    }

    mmapfs_journal_commit();
    MMAPFS_UNLOCK();
    return 0;
}

/* ---- File metadata ---- */

int nufs_stat(const char *pathname, struct stat *statbuf) {
    MMAPFS_LOCK();
    uint32_t ino = mmapfs_path_resolve(pathname);
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }
    fill_stat(statbuf, ino, mmapfs_inode_get(ino));
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_lstat(const char *pathname, struct stat *statbuf) {
    /* Symlinks are stored as regular inodes with S_IFLNK; lstat
     * returns info about the link itself, same as stat for now. */
    return nufs_stat(pathname, statbuf);
}

int nufs_fstat(int fd, struct stat *statbuf) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }
    fill_stat(statbuf, fde->ino, mmapfs_inode_get(fde->ino));
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_fallocate(int fd, int mode, off_t offset, off_t len) {
    (void)mode;
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    uint64_t end = (uint64_t)offset + (uint64_t)len;

    /* Allocate blocks for the requested range */
    uint32_t start_blk = (uint64_t)offset / MMAPFS_BLOCK_SIZE;
    uint32_t end_blk = (end + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    for (uint32_t b = start_blk; b < end_blk; b++) {
        if (mmapfs_inode_get_block(inode, b) == 0) {
            uint32_t blk = mmapfs_inode_alloc_block(fde->ino, b);
            if (blk == 0) {
                MMAPFS_UNLOCK();
                return -ENOSPC;
            }
            inode = mmapfs_inode_get(fde->ino);
        }
    }

    if (end > inode->i_size)
        inode->i_size = end;
    inode->i_mtime = time(NULL);
    inode->i_ctime = inode->i_mtime;
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_truncate(const char *pathname, off_t length) {
    MMAPFS_LOCK();
    uint32_t ino = mmapfs_path_resolve(pathname);
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }
    int rc = mmapfs_inode_truncate(ino, length);
    MMAPFS_UNLOCK();
    return rc;
}

int nufs_ftruncate(int fd, off_t length) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }
    int rc = mmapfs_inode_truncate(fde->ino, length);
    MMAPFS_UNLOCK();
    return rc;
}

/* ---- Unlink / Symlink ---- */

int nufs_unlink(const char *pathname) {
    MMAPFS_LOCK();

    char name[MMAPFS_NAME_MAX];
    uint32_t parent = mmapfs_path_resolve_parent(pathname, name);
    if (parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    uint32_t ino = mmapfs_dir_lookup(parent, name);
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    struct mmapfs_inode *inode = mmapfs_inode_get(ino);
    if (S_ISDIR(inode->i_mode)) {
        MMAPFS_UNLOCK();
        return -EISDIR;
    }

    mmapfs_journal_begin();

    mmapfs_dir_remove(parent, name);
    if (inode->i_nlink > 0)
        inode->i_nlink--;

    if (inode->i_nlink == 0) {
        mmapfs_inode_truncate(ino, 0);
        mmapfs_free_inode(ino);
    }

    struct mmapfs_inode *pnode = mmapfs_inode_get(parent);
    pnode->i_mtime = time(NULL);
    pnode->i_ctime = pnode->i_mtime;

    struct mmapfs_state *st = mmapfs_get_state();
    mmapfs_journal_write_block(
        st->super->s_inode_bitmap,
        mmapfs_get_block(st->super->s_inode_bitmap));

    mmapfs_journal_commit();
    MMAPFS_UNLOCK();
    return 0;
}

int nufs_symlink(const char *target, const char *linkpath) {
    MMAPFS_LOCK();

    char name[MMAPFS_NAME_MAX];
    uint32_t parent = mmapfs_path_resolve_parent(linkpath, name);
    if (parent == 0) {
        MMAPFS_UNLOCK();
        return -ENOENT;
    }

    uint32_t ino = mmapfs_alloc_inode();
    if (ino == 0) {
        MMAPFS_UNLOCK();
        return -ENOSPC;
    }

    mmapfs_inode_init(ino, S_IFLNK | 0777);
    struct mmapfs_inode *inode = mmapfs_inode_get(ino);

    /* Store the symlink target in a data block */
    size_t tlen = strlen(target);
    if (tlen >= MMAPFS_BLOCK_SIZE)
        tlen = MMAPFS_BLOCK_SIZE - 1;

    uint32_t blk = mmapfs_alloc_block();
    if (blk == 0) {
        mmapfs_free_inode(ino);
        MMAPFS_UNLOCK();
        return -ENOSPC;
    }
    inode->i_direct[0] = blk;
    inode->i_blocks = 1;
    inode->i_size = tlen;

    char *data = (char *)mmapfs_get_block(blk);
    memcpy(data, target, tlen);
    data[tlen] = '\0';

    int rc = mmapfs_dir_add(parent, name, ino);
    if (rc < 0) {
        mmapfs_free_block(blk);
        mmapfs_free_inode(ino);
        MMAPFS_UNLOCK();
        return rc;
    }

    MMAPFS_UNLOCK();
    return 0;
}

/* ---- Miscellaneous ---- */

int nufs_access(const char *pathname, int mode) {
    (void)mode;
    MMAPFS_LOCK();
    uint32_t ino = mmapfs_path_resolve(pathname);
    MMAPFS_UNLOCK();
    if (ino == 0)
        return -ENOENT;
    return 0;
}

int nufs_fsync(int fd) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    /* Flush all blocks belonging to this file */
    struct mmapfs_inode *inode = mmapfs_inode_get(fde->ino);
    uint32_t nblocks =
        (inode->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;
    for (uint32_t i = 0; i < nblocks; i++) {
        uint32_t blk = mmapfs_inode_get_block(inode, i);
        if (blk != 0)
            mmapfs_sync_block(blk);
    }

    /* Also sync the inode itself */
    struct mmapfs_state *st = mmapfs_get_state();
    uint32_t itbl_blk = st->super->s_inode_table +
                         fde->ino / MMAPFS_INODES_PER_BLOCK;
    mmapfs_sync_block(itbl_blk);

    MMAPFS_UNLOCK();
    return 0;
}

int nufs_fdatasync(int fd) {
    /* Same as fsync for this implementation */
    return nufs_fsync(fd);
}

int nufs_fcntl(int fd, int cmd, void *arg) {
    MMAPFS_LOCK();
    struct mmapfs_fd *fde = fd_get(fd);
    if (!fde) {
        MMAPFS_UNLOCK();
        return -EBADF;
    }

    int ret;
    switch (cmd) {
    case F_GETFL:
        ret = fde->flags;
        break;
    case F_SETFL:
        /* Cast via long to safely convert a pointer-width value to int,
         * matching the glibc convention where fcntl varargs pass int
         * values widened to long/pointer size. */
        fde->flags = (int)(long)arg;
        ret = 0;
        break;
    case F_GETFD:
        ret = 0;
        break;
    case F_SETFD:
        ret = 0;
        break;
    default:
        ret = -EINVAL;
        break;
    }

    MMAPFS_UNLOCK();
    return ret;
}
