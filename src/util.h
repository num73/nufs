#ifndef UTIL_H
#define UTIL_H

#define NUFS_PATH_PREFIX (char *)"/mnt/nufs"
#define NUFS_PATH_PREFIX_LEN 14

#define NUFS_PATH_CHECK(PATH) \
	(!strncmp(PATH, NUFS_PATH_PREFIX, NUFS_PATH_PREFIX_LEN))
#define ABSOLUTE_PATH_CHECK(PATH) (PATH[0] == '/')

#define NUFS_FD_PREFIX 0x1000000

#define NUFS_FD_CHECK(FD) (FD & NUFS_FD_PREFIX)

void util_get_fullpath(const char *path, char *fullpath);

#endif /* UTIL_H */
