#include "nufs.h"
#include "util.h"
#include <dlfcn.h>
#include <libsyscall_intercept_hook_point.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <syscall.h>
/* glibc syscall hooks */
int shim_do_open(const char *pathname, int flags, mode_t mode, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    // todo
    ret = nufs_open(pathname, flags, mode);

    *result = ret;
    return 0;
}

int shim_do_openat(int dfd, const char *pathname, int flags, mode_t mode,
                   int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = (char *)pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_open((char *)pathname, flags, mode);

    *result = ret;
    return 0;
}

int shim_do_create(const char *pathname, mode_t mode, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_creat(pathname, mode);

    *result = ret;
    return 0;
}

int shim_do_read(int fd, void *buf, size_t count, ssize_t *result) {
    ssize_t ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    
    ret = nufs_read(fd, buf, count);

    *result = ret;
    return 0;
}

int shim_do_pread64(int fd, void *buf, size_t count, off_t off,
                    ssize_t *result) {
    ssize_t ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_pread(fd, buf, count, off);

    *result = ret;
    return 0;
}

int shim_do_write(int fd, void *buf, size_t count, ssize_t *result) {
    ssize_t ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_write(fd, buf, count);

    *result = ret;
    return 0;
}

int shim_do_pwrite64(int fd, void *buf, size_t count, off_t off,
                     ssize_t *result) {
    ssize_t ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_pwrite(fd, buf, count, off);

    *result = ret;
    return 0;
}

int shim_do_close(int fd, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_close(fd);

    *result = ret;
    return 0;
}

int shim_do_lseek(int fd, off_t offset, int origin, off_t *result) {
    off_t ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_lseek(fd, offset, origin);

    *result = ret;
    return 0;
}

int shim_do_mkdir(const char *pathname, mode_t mode, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_mkdir(pathname, mode);

    *result = ret;
    return 0;
}

int shim_do_rmdir(const char *pathname, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_rmdir(pathname);

    *result = ret;
    return 0;
}

int shim_do_rename(const char *oldname, const char *newname, int *result) {
    int ret = 0;
    char oldpathtmp[PATH_MAX];
    char newpathtmp[PATH_MAX];
    const char *fulloldpath = NULL;
    const char *fullnewpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(oldname)) {
        fulloldpath = oldname;
    } else {
        nu_get_fullpath(oldname, oldpathtmp);
        fulloldpath = oldpathtmp;
    }

    if (ABSOLUTE_PATH_CHECK(newname)) {
        fullnewpath = newname;
    } else {
        nu_get_fullpath(newname, newpathtmp);
        fullnewpath = newpathtmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fulloldpath) && !NUFS_PATH_CHECK(fullnewpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_rename(oldname, newname);

    *result = ret;
    return 0;
}

int shim_do_fallocate(int fd, int mode, off_t offset, off_t len, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_fallocate(fd, mode, offset, len);

    *result = ret;
    return 0;
}

int shim_do_stat(const char *pathname, struct stat *statbuf, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_stat(pathname, statbuf);

    *result = ret;
    return 0;
}

int shim_do_lstat(const char *pathname, struct stat *statbuf, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    // todo
    ret = nufs_lstat(pathname, statbuf);

    *result = ret;
    return 0;
}

int shim_do_fstat(int fd, struct stat *statbuf, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_fstat(fd, statbuf);

    *result = ret;
    return 0;
}

int shim_do_fstatat(int dirfd, const char *pathname, struct stat *statbuf,
                    int flags, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* If path is empty, call fstat() instead */
    if (strlen(pathname) == 0)
        return shim_do_fstat(dirfd, statbuf, result);

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_stat(pathname, statbuf);

    *result = ret;
    return 0;
}

int shim_do_truncate(const char *pathname, off_t length, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_truncate(pathname, length);

    *result = ret;
    return 0;
}

int shim_do_ftruncate(int fd, off_t length, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_ftruncate(fd, length);

    *result = ret;
    return 0;
}

int shim_do_unlink(const char *pathname, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_unlink(pathname);

    *result = ret;
    return 0;
}

int shim_do_symlink(const char *target, const char *linkpath, int *result) {
    int ret = 0;
    char targettmp[PATH_MAX];
    char linkpathtmp[PATH_MAX];
    const char *fulltarget = NULL;
    const char *fulllinkpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(target)) {
        fulltarget = target;
    } else {
        nu_get_fullpath(target, targettmp);
        fulltarget = targettmp;
    }

    if (ABSOLUTE_PATH_CHECK(linkpath)) {
        fulllinkpath = linkpath;
    } else {
        nu_get_fullpath(linkpath, linkpathtmp);
        fulllinkpath = linkpathtmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fulltarget) && !NUFS_PATH_CHECK(fulllinkpath)) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_symlink(target, linkpath);

    *result = ret;
    return 0;
}

int shim_do_access(const char *pathname, int mode, int *result) {
    int ret = 0;
    char tmp[PATH_MAX];
    const char *fullpath = NULL;

    /* Get file full path */
    if (ABSOLUTE_PATH_CHECK(pathname)) {
        fullpath = pathname;
    } else {
        nu_get_fullpath(pathname, tmp);
        fullpath = tmp;
    }

    /* Check if it is a regular path, route to real syscall */
    if (!NUFS_PATH_CHECK(fullpath)) {
        return 1;
    }

    /* Go through NUFS */
    // todo
    ret = nufs_access(pathname, mode);

    *result = ret;
    return 0;
}

int shim_do_fsync(int fd, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_fsync(fd);

    *result = ret;
    return 0;
}

int shim_do_fdatasync(int fd, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_fdatasync(fd);

    *result = ret;
    return 0;
}

int shim_do_sync(int *result) { return 1; }

int shim_do_fcntl(int fd, int cmd, void *arg, int *result) {
    int ret = 0;

    /* Check if it is a regular fd */
    if (NUFS_FD_CHECK(fd) == 0) {
        return 1;
    }

    /* Go through NUFS */
    ret = nufs_fcntl(fd, cmd, arg);

    *result = ret;
    return 0;
}

int shim_do_mmap(void *addr, size_t length, int prot, int flags, int fd,
                 off_t offset, void **result) {
    return 1;
}

int shim_do_munmap(void *addr, size_t length, int *result) { return 1; }

static int hook(long syscall_number, long arg0, long arg1, long arg2, long arg3,
                long arg4, long arg5, long *result) {
    switch (syscall_number) {
    case SYS_open:
        return shim_do_open((const char *)arg0, (int)arg1, (mode_t)arg2,
                            (int *)result);
    case SYS_openat:
        return shim_do_openat((int)arg0, (const char *)arg1, (int)arg2,
                              (mode_t)arg3, (int *)result);
    case SYS_creat:
        return shim_do_create((char *)arg0, (mode_t)arg1, (int *)result);
    case SYS_read:
        return shim_do_read((int)arg0, (void *)arg1, (size_t)arg2,
                            (ssize_t *)result);
    case SYS_pread64:
        return shim_do_pread64((int)arg0, (void *)arg1, (size_t)arg2,
                               (off_t)arg3, (ssize_t *)result);
    case SYS_write:
        return shim_do_write((int)arg0, (void *)arg1, (size_t)arg2,
                             (ssize_t *)result);
    case SYS_pwrite64:
        return shim_do_pwrite64((int)arg0, (void *)arg1, (size_t)arg2,
                                (off_t)arg3, (ssize_t *)result);
    case SYS_close:
        return shim_do_close((int)arg0, (int *)result);
    case SYS_lseek:
        return shim_do_lseek((int)arg0, (off_t)arg1, (int)arg2,
                             (off_t *)result);
    case SYS_mkdir:
        return shim_do_mkdir((const char *)arg0, (mode_t)arg1, (int *)result);
    case SYS_rmdir:
        return shim_do_rmdir((const char *)arg0, (int *)result);
    case SYS_rename:
        return shim_do_rename((const char *)arg0, (const char *)arg1,
                              (int *)result);
    case SYS_fallocate:
        return shim_do_fallocate((int)arg0, (int)arg1, (off_t)arg2, (off_t)arg3,
                                 (int *)result);
    case SYS_stat:
        return shim_do_stat((const char *)arg0, (struct stat *)arg1,
                            (int *)result);
    case SYS_newfstatat:
        return shim_do_fstatat((int)arg0, (const char *)arg1,
                               (struct stat *)arg2, (int)arg3, (int *)result);
    case SYS_lstat:
        return shim_do_lstat((const char *)arg0, (struct stat *)arg1,
                             (int *)result);

    case SYS_fstat:
        return shim_do_fstat((int)arg0, (struct stat *)arg1, (int *)result);
    case SYS_truncate:
        return shim_do_truncate((const char *)arg0, (off_t)arg1, (int *)result);
    case SYS_ftruncate:
        return shim_do_ftruncate((int)arg0, (off_t)arg1, (int *)result);
    case SYS_unlink:
        return shim_do_unlink((const char *)arg0, (int *)result);
    case SYS_symlink:

        return shim_do_symlink((const char *)arg0, (const char *)arg1,
                               (int *)result);
    case SYS_access:
        return shim_do_access((const char *)arg0, (int)arg1, (int *)result);
    case SYS_fsync:
        return shim_do_fsync((int)arg0, (int *)result);
    case SYS_fdatasync:
        return shim_do_fdatasync((int)arg0, (int *)result);
    case SYS_sync:
        return shim_do_sync((int *)result);
    case SYS_fcntl:
        return shim_do_fcntl((int)arg0, (int)arg1, (void *)arg2, (int *)result);
    case SYS_mmap:
        return shim_do_mmap((void *)arg0, (size_t)arg1, (int)arg2, (int)arg3,
                            (int)arg4, (off_t)arg5, (void **)result);
    case SYS_munmap:
        return shim_do_munmap((void *)arg0, (size_t)arg1, (int *)result);
    }

    return 1;
}

static __attribute__((constructor)) void init(void) {
    /* Set up the callback function */
    intercept_hook_point = hook;

    nufs_init();
}