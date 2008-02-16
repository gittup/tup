#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#define __USE_GNU
#include <dlfcn.h>

#include "file.h"

#define HANDLE_FILE(f, rw) handle_file(f, rw, __func__);

int open(const char *pathname, int flags, ...)
{
	int (*s_open)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");
	int rc;

	mode_t mode = 0;
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	/* O_ACCMODE is 0x3, which covers O_WRONLY and O_RDWR */
	rc = s_open(pathname, flags, mode);
	if(rc >= 0)
		HANDLE_FILE(pathname, flags&O_ACCMODE);
	return rc;
}

int open64(const char *pathname, int flags, ...)
{
	int (*s_open64)(const char *, int, ...) = dlsym(RTLD_NEXT, "open64");
	int rc;

	mode_t mode = 0;
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	rc = s_open64(pathname, flags, mode);
	if(rc >= 0)
		HANDLE_FILE(pathname, flags&O_ACCMODE);
	return rc;
}

FILE *fopen(const char *path, const char *mode)
{
	FILE *(*s_fopen)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
	FILE *f;

	f = s_fopen(path, mode);
	if(f)
		HANDLE_FILE(path, !(mode[0] == 'r'));
	return f;
}

FILE *fopen64(const char *path, const char *mode)
{
	FILE *(*s_fopen64)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen64");
	FILE *f;

	f = s_fopen64(path, mode);
	if(f)
		HANDLE_FILE(path, !(mode[0] == 'r'));
	return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
	FILE *f;
	FILE *(*s_freopen)(const char *, const char *, FILE *) = dlsym(RTLD_NEXT, "freopen");

	f = s_freopen(path, mode, stream);
	if(f)
		HANDLE_FILE(path, !(mode[0] == 'r'));
	return f;
}

int creat(const char *pathname, mode_t mode)
{
	int rc;
	int (*s_creat)(const char *, mode_t) = dlsym(RTLD_NEXT, "creat");

	rc = s_creat(pathname, mode);
	if(rc >= 0)
		HANDLE_FILE(pathname, 1);
	return rc;
}

int rename(const char *old, const char *new)
{
	int rc;
	int (*s_rename)(const char*, const char*) = dlsym(RTLD_NEXT, "rename");

	rc = s_rename(old, new);
	if(rc == 0)
		fprintf(stderr, "RENAMED %s to %s\n", old, new);
	return rc;
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
	int rc;
	int (*s_execve)(const char *, char *const[], char *const[]) = dlsym(RTLD_NEXT, "execve");

	fprintf(stderr, "EXECVE '%s'\n", filename);
	rc = s_execve(filename, argv, envp);
	return rc;
}
