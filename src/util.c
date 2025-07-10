#include "util.h"
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


void nu_get_fullpath(const char *path, char *fullpath) {
    /* Get file full path */
    memset(fullpath, 0, PATH_MAX);
    if (path[0] == '/') {
        strcpy(fullpath, path);
    } else {
        getcwd(fullpath, PATH_MAX);
        strcat(fullpath, "/");
        strcat(fullpath, path);
    }
}

int64_t nu_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * _1SEC_NS + ts.tv_nsec;
}