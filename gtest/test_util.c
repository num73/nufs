#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "util.h"

static void test_util_get_fullpath(void) {

    char *fullpath = malloc(PATH_MAX);
    nu_get_fullpath("test.txt", fullpath);
    g_assert_nonnull(fullpath);
    g_assert_cmpstr(fullpath, ==, "/home/num73/nufs/build/gtest/test.txt");
    g_free(fullpath);
}

int main(int argc, char *argv[]) {

    g_test_init(&argc, &argv, NULL);

    g_test_set_nonfatal_assertions();

    g_test_add_func("/util/get_fullpath", test_util_get_fullpath);
    
    return g_test_run();
}