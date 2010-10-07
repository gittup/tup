#include "dirpath.h"

int __wrap_dup(int oldfd);
int __real_dup(int oldfd);

int __wrap_dup(int oldfd)
{
	int rc;

	rc = win32_dup(oldfd);
	/* -1 means we should've win32_dup'd it but it failed */
	if(rc == -1)
		return -1;
	if(rc > 0)
		return rc;
	return __real_dup(oldfd);
}
