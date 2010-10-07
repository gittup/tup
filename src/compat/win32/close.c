#include "dirpath.h"

int __wrap_close(int fd);
int __real_close(int fd);

int __wrap_close(int fd)
{
	win32_rm_dirpath(fd);
	return 0;
}
