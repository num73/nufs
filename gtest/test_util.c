#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "util.h"

static int tests_run = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n  assertion: %s\n", msg, #cond); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while (0)

static void test_util_get_fullpath_relative(void) {
    char fullpath[PATH_MAX];
    char cwd[PATH_MAX];
    char expected[PATH_MAX];

    getcwd(cwd, sizeof(cwd));
    strcpy(expected, cwd);
    strcat(expected, "/test.txt");

    nu_get_fullpath("test.txt", fullpath);

    TEST_ASSERT(fullpath[0] == '/', "fullpath is absolute");
    TEST_ASSERT(strcmp(fullpath, expected) == 0, "relative path resolved correctly");
}

static void test_util_get_fullpath_absolute(void) {
    char fullpath[PATH_MAX];
    const char *input = "/tmp/test.txt";

    nu_get_fullpath(input, fullpath);

    TEST_ASSERT(strcmp(fullpath, input) == 0, "absolute path preserved");
}

int main(void) {
    printf("Running util tests...\n");

    test_util_get_fullpath_relative();
    test_util_get_fullpath_absolute();

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}