#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#define __USE_GNU
#include <dlfcn.h>

#define HANDLE_FILE(f, rw) handle_file(f, rw, __func__);

/* 'file' is the full filename, rw is 1 for write, 0 for read */
static void handle_file(const char *file, int rw, const char *func);

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

static void handle_file(const char *file, int rw, const char *func)
{
	if(strncmp(file, "/tmp/", 5) == 0) {
		return;
	}
	fprintf(stderr, "MARF: File '%s' in mode %i from %s\n", file, rw, func);
}
