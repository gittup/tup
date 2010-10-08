#include "dirpath.h"

int __wrap_close(int fd);
int __real_close(int fd);

int __wrap_close(int fd)
{
	if(!win32_rm_dirpath(fd))
		__real_close(fd);
	return 0;
}
