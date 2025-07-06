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

#endif /* NUFS_H */