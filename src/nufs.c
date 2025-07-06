#include "nufs.h"
#include <stdio.h>

void display_nufs_info(void)
{
	printf("NUFS Path Prefix: %s\n", NUFS_PATH_PREFIX);

	printf("NUFS Path Prefix Length: %d\n", NUFS_PATH_PREFIX_LEN);

	printf("NUFS FD Prefix: 0x%x\n", NUFS_FD_PREFIX);
}

void nufs_init(void)
{
	display_nufs_info();
	// Initialization code for NUFS can be added here
	printf("NUFS initialized.\n");
}