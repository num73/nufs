/*
 * dir.c - Directory operations for mmapfs
 *
 * Directories are stored as a flat array of fixed-size entries (256 bytes
 * each, 16 per block).  An entry with d_ino == 0 is considered free.
 *
 * Also provides path resolution utilities that strip the /mnt/nufs
 * prefix and walk the directory tree.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <linux/limits.h>
#include <string.h>

/* ---- Path utilities ---- */

/* Strip the NUFS mount-point prefix (/mnt/nufs) and return the
 * filesystem-internal path.  If the path does not have the prefix
 * it is returned unchanged. */
const char *mmapfs_strip_prefix(const char *path) {
    if (strncmp(path, "/mnt/nufs", 9) != 0)
        return path;
    path += 9;
    if (*path == '\0')
        return "/";
    return path; /* e.g. "/dir/file" */
}

/* Internal helper: resolve an already-stripped path to an inode number.
 * Returns MMAPFS_BAD_INO (0) if any component is not found. */
static uint32_t resolve_internal(const char *ipath) {
    uint32_t ino = MMAPFS_ROOT_INO;
    char buf[PATH_MAX];

    strncpy(buf, ipath, PATH_MAX - 1);
    buf[PATH_MAX - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, "/", &saveptr);

    while (tok) {
        if (tok[0] == '\0') {
            tok = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        ino = mmapfs_dir_lookup(ino, tok);
        if (ino == 0)
            return 0;
        tok = strtok_r(NULL, "/", &saveptr);
    }
    return ino;
}

/* Resolve a full NUFS path (e.g. "/mnt/nufs/dir/file") to an inode.
 * Returns 0 if not found. */
uint32_t mmapfs_path_resolve(const char *path) {
    return resolve_internal(mmapfs_strip_prefix(path));
}

/* Resolve the parent directory of the given path and copy the last
 * path component (file/dir name) into name_out.
 * Returns the parent inode number, or 0 on failure. */
uint32_t mmapfs_path_resolve_parent(const char *path, char *name_out) {
    const char *ipath = mmapfs_strip_prefix(path);
    char buf[PATH_MAX];

    strncpy(buf, ipath, PATH_MAX - 1);
    buf[PATH_MAX - 1] = '\0';

    /* Remove trailing slashes */
    size_t len = strlen(buf);
    while (len > 1 && buf[len - 1] == '/')
        buf[--len] = '\0';

    /* Find last slash to separate parent path and basename */
    char *slash = strrchr(buf, '/');
    if (!slash) {
        /* No slash – treat as child of root */
        strncpy(name_out, buf, MMAPFS_NAME_MAX - 1);
        name_out[MMAPFS_NAME_MAX - 1] = '\0';
        return MMAPFS_ROOT_INO;
    }

    /* Copy basename */
    strncpy(name_out, slash + 1, MMAPFS_NAME_MAX - 1);
    name_out[MMAPFS_NAME_MAX - 1] = '\0';

    if (slash == buf) {
        /* Parent is root (e.g. path was "/file") */
        return MMAPFS_ROOT_INO;
    }

    /* Terminate the parent portion and resolve it */
    *slash = '\0';
    return resolve_internal(buf);
}

/* ---- Directory entry operations ---- */

/* Look up a name inside a directory.  Returns the inode number of the
 * entry, or 0 if not found. */
uint32_t mmapfs_dir_lookup(uint32_t dir_ino, const char *name) {
    struct mmapfs_inode *dir = mmapfs_inode_get(dir_ino);
    if (!S_ISDIR(dir->i_mode))
        return 0;

    uint32_t nblocks =
        (dir->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t blk = mmapfs_inode_get_block(dir, b);
        if (blk == 0)
            continue;

        struct mmapfs_dirent *de =
            (struct mmapfs_dirent *)mmapfs_get_block(blk);
        for (uint32_t i = 0; i < MMAPFS_DIRENTS_PER_BLOCK; i++) {
            if (de[i].d_ino != 0 &&
                strncmp(de[i].d_name, name, MMAPFS_NAME_MAX) == 0)
                return de[i].d_ino;
        }
    }
    return 0;
}

/* Add a new entry to a directory.  Searches for an empty slot first;
 * if none is found, allocates a new block for the directory. */
int mmapfs_dir_add(uint32_t dir_ino, const char *name, uint32_t ino) {
    struct mmapfs_inode *dir = mmapfs_inode_get(dir_ino);
    if (!S_ISDIR(dir->i_mode))
        return -ENOTDIR;

    /* Reject duplicate names */
    if (mmapfs_dir_lookup(dir_ino, name) != 0)
        return -EEXIST;

    uint32_t nblocks =
        (dir->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    /* Search for an empty slot in existing blocks */
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t blk = mmapfs_inode_get_block(dir, b);
        if (blk == 0)
            continue;

        struct mmapfs_dirent *de =
            (struct mmapfs_dirent *)mmapfs_get_block(blk);
        for (uint32_t i = 0; i < MMAPFS_DIRENTS_PER_BLOCK; i++) {
            if (de[i].d_ino == 0) {
                de[i].d_ino = ino;
                memset(de[i].d_name, 0, MMAPFS_NAME_MAX);
                strncpy(de[i].d_name, name, MMAPFS_NAME_MAX - 1);
                return 0;
            }
        }
    }

    /* No empty slot – allocate a new block for the directory */
    uint32_t new_blk = mmapfs_inode_alloc_block(dir_ino, nblocks);
    if (new_blk == 0)
        return -ENOSPC;

    /* Re-read the inode pointer (may have moved if alloc changed things) */
    dir = mmapfs_inode_get(dir_ino);
    dir->i_size += MMAPFS_BLOCK_SIZE;

    struct mmapfs_dirent *de =
        (struct mmapfs_dirent *)mmapfs_get_block(new_blk);
    de[0].d_ino = ino;
    memset(de[0].d_name, 0, MMAPFS_NAME_MAX);
    strncpy(de[0].d_name, name, MMAPFS_NAME_MAX - 1);
    return 0;
}

/* Remove a named entry from a directory.  The entry is marked free by
 * setting d_ino to 0 (the name is also cleared). */
int mmapfs_dir_remove(uint32_t dir_ino, const char *name) {
    struct mmapfs_inode *dir = mmapfs_inode_get(dir_ino);
    if (!S_ISDIR(dir->i_mode))
        return -ENOTDIR;

    uint32_t nblocks =
        (dir->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t blk = mmapfs_inode_get_block(dir, b);
        if (blk == 0)
            continue;

        struct mmapfs_dirent *de =
            (struct mmapfs_dirent *)mmapfs_get_block(blk);
        for (uint32_t i = 0; i < MMAPFS_DIRENTS_PER_BLOCK; i++) {
            if (de[i].d_ino != 0 &&
                strncmp(de[i].d_name, name, MMAPFS_NAME_MAX) == 0) {
                de[i].d_ino = 0;
                memset(de[i].d_name, 0, MMAPFS_NAME_MAX);
                return 0;
            }
        }
    }
    return -ENOENT;
}

/* Check whether a directory is empty (contains only "." and ".."). */
int mmapfs_dir_is_empty(uint32_t dir_ino) {
    struct mmapfs_inode *dir = mmapfs_inode_get(dir_ino);
    uint32_t nblocks =
        (dir->i_size + MMAPFS_BLOCK_SIZE - 1) / MMAPFS_BLOCK_SIZE;

    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t blk = mmapfs_inode_get_block(dir, b);
        if (blk == 0)
            continue;

        struct mmapfs_dirent *de =
            (struct mmapfs_dirent *)mmapfs_get_block(blk);
        for (uint32_t i = 0; i < MMAPFS_DIRENTS_PER_BLOCK; i++) {
            if (de[i].d_ino == 0)
                continue;
            if (strcmp(de[i].d_name, ".") == 0 ||
                strcmp(de[i].d_name, "..") == 0)
                continue;
            return 0; /* Not empty */
        }
    }
    return 1; /* Empty */
}
