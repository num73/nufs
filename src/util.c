#include "util.h"
#include <string.h>
#include <unistd.h>
#include <limits.h>

void util_get_fullpath(const char *path, char *fullpath)
{
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