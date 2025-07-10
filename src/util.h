#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#define _1SEC_NS (1000000000L)

void nu_get_fullpath(const char *path, char *fullpath);

int64_t nu_get_time_ns(void);

#endif /* UTIL_H */
