#include "tup/flock.h"
#include <windows.h>
#include <errno.h>

int tup_flock(int fd)
{
	HANDLE h;
	long int tmp;
	OVERLAPPED wtf;

	tmp = _get_osfhandle(fd);
	h = (HANDLE)tmp;

	memset(&wtf, 0, sizeof(wtf));
	if(LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &wtf) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_unflock(int fd)
{
	HANDLE h;
	long int tmp;

	tmp = _get_osfhandle(fd);
	h = (HANDLE)tmp;
	if(UnlockFile(h, 0, 0, 1, 0) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_wait_flock(int fd)
{
	if(fd) {}
	/* Unsupported - only used by inotify file monitor */
	return -1;
}
