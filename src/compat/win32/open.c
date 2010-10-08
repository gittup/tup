#include <stdio.h>
#include <fcntl.h>
#include <windows.h>
#include "dirpath.h"

int __wrap_open(const char *pathname, int flags, ...);
int __real_open(const char *pathname, int flags, ...);

int __wrap_open(const char *pathname, int flags, ...)
{
	mode_t mode = 0;

	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		DWORD attributes;

		attributes = GetFileAttributesA(pathname);

		if(attributes == INVALID_FILE_ATTRIBUTES) {
			fprintf(stderr, "Error getting file attributes for file '%s'\n", pathname);
			return -1;
		}
		if(attributes & FILE_ATTRIBUTE_DIRECTORY) {
			return win32_add_dirpath(pathname);
		}
	}
	return __real_open(pathname, flags, mode);
}
