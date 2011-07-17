#include <stdio.h>
#include "open_notify.h"

int __wrap_stat(const char *path, struct stat *buf);
int __real_stat(const char *path, struct stat *buf);

int __wrap_stat(const char *path, struct stat *buf)
{
	if(open_notify(ACCESS_READ, path) < 0)
		return -1;
	return __real_stat(path, buf);
}
