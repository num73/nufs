#ifndef NUFS_H
#define NUFS_H

#include <linux/limits.h>
#include <sys/types.h>

#define NUFS_PATH_PREFIX (char *)"/mnt/nufs"

#define NUFS_PATH_PREFIX_LEN 9

#define NUFS_PATH_CHECK(PATH)                                                  \
    (!strncmp(PATH, NUFS_PATH_PREFIX, NUFS_PATH_PREFIX_LEN))

#define ABSOLUTE_PATH_CHECK(PATH) (PATH[0] == '/')

#define NUFS_FD_PREFIX 0x1000000

#define NUFS_FD_CHECK(FD) (FD & NUFS_FD_PREFIX)


void nufs_init(void);

int nufs_open(const char *pathname, int flags, mode_t mode);

int nufs_creat(const char *pathname, mode_t mode);

ssize_t nufs_read(int fd, void *buf, size_t count);

ssize_t nufs_pread(int fd, void *buf, size_t count, off_t offset);

ssize_t nufs_write(int fd, const void *buf, size_t count);

ssize_t nufs_pwrite(int fd, const void *buf, size_t count, off_t offset);

int nufs_close(int fd);

off_t nufs_lseek(int fd, off_t offset, int whence);

int nufs_mkdir(const char *pathname, mode_t mode);

int nufs_rmdir(const char *pathname);

int nufs_rename(const char *oldname, const char *newname);

int nufs_fallocate(int fd, int mode, off_t offset, off_t len);

int nufs_stat(const char *pathname, struct stat *statbuf);

int nufs_lstat(const char *pathname, struct stat *statbuf);

int nufs_fstat(int fd, struct stat *statbuf);

int nufs_truncate(const char *pathname, off_t length);

int nufs_ftruncate(int fd, off_t length);

int nufs_unlink(const char *pathname);

int nufs_symlink(const char *target, const char *linkpath);

int nufs_access(const char *pathname, int mode);

int nufs_fsync(int fd);

int nufs_fdatasync(int fd);

int nufs_fcntl(int fd, int cmd, void *arg);
void nufs_init(void);
#endif /* NUFS_H */
