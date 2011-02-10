#define _GNU_SOURCE
#include "tup/access_event.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>

#if defined(__APPLE__)
#include <sys/stat.h>
#endif

static void handle_file(const char *file, const char *file2, int at);
static int ignore_file(const char *file);

static int (*s_open)(const char *, int, ...);
static int (*s_open64)(const char *, int, ...);
static FILE *(*s_fopen)(const char *, const char *);
static FILE *(*s_fopen64)(const char *, const char *);
static FILE *(*s_freopen)(const char *, const char *, FILE *);
static int (*s_creat)(const char *, mode_t);
static int (*s_symlink)(const char *, const char *);
static int (*s_rename)(const char*, const char*);
static int (*s_mkstemp)(char *template);
static int (*s_mkostemp)(char *template, int flags);
static int (*s_unlink)(const char*);
static int (*s_unlinkat)(int, const char*, int);
static int (*s_execve)(const char *filename, char *const argv[],
		       char *const envp[]);
static int (*s_execv)(const char *path, char *const argv[]);
static int (*s_execvp)(const char *file, char *const argv[]);
static int (*s_chdir)(const char *path);
static int (*s_xstat)(int vers, const char *name, struct stat *buf);
#if defined(__APPLE__)
static int (*s_stat)(const char *name, struct stat *buf);
#endif
static int (*s_stat64)(const char *name, struct stat64 *buf);
static int (*s_xstat64)(int vers, const char *name, struct stat64 *buf);
static int (*s_lxstat64)(int vers, const char *path, struct stat64 *buf);

#define WRAP(ptr, name) \
	if(!ptr) { \
		ptr = dlsym(RTLD_NEXT, name); \
		if(!ptr) { \
			fprintf(stderr, "tup.ldpreload: Unable to wrap '%s'\n", \
				name); \
			exit(1); \
		} \
	}

int open(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;

	WRAP(s_open, "open");
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	rc = s_open(pathname, flags, mode);
	if(rc >= 0) {
		int at = ACCESS_READ;

		if(flags&O_WRONLY || flags&O_RDWR)
			at = ACCESS_WRITE;
		handle_file(pathname, "", at);
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(pathname, "", ACCESS_GHOST);
	}
	return rc;
}

int open64(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;

	WRAP(s_open64, "open64");
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	rc = s_open64(pathname, flags, mode);
	if(rc >= 0) {
		int at = ACCESS_READ;

		if(flags&O_WRONLY || flags&O_RDWR)
			at = ACCESS_WRITE;
		handle_file(pathname, "", at);
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(pathname, "", ACCESS_GHOST);
	}
	return rc;
}

FILE *fopen(const char *path, const char *mode)
{
	FILE *f;

	WRAP(s_fopen, "fopen");
	f = s_fopen(path, mode);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

FILE *fopen64(const char *path, const char *mode)
{
	FILE *f;

	WRAP(s_fopen64, "fopen64");
	f = s_fopen64(path, mode);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
	FILE *f;

	WRAP(s_freopen, "freopen");
	f = s_freopen(path, mode, stream);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

int creat(const char *pathname, mode_t mode)
{
	int rc;

	WRAP(s_creat, "creat");
	rc = s_creat(pathname, mode);
	if(rc >= 0)
		handle_file(pathname, "", ACCESS_WRITE);
	return rc;
}

int symlink(const char *oldpath, const char *newpath)
{
	int rc;
	WRAP(s_symlink, "symlink");
	rc = s_symlink(oldpath, newpath);
	if(rc == 0)
		handle_file(oldpath, newpath, ACCESS_SYMLINK);
	return rc;
}

int rename(const char *old, const char *new)
{
	int rc;

	WRAP(s_rename, "rename");
	rc = s_rename(old, new);
	if(rc == 0) {
		if(!ignore_file(old) && !ignore_file(new)) {
			handle_file(old, new, ACCESS_RENAME);
		}
	}
	return rc;
}

int mkstemp(char *template)
{
	int rc;

	WRAP(s_mkstemp, "mkstemp");
	rc = s_mkstemp(template);
	if(rc != -1) {
		handle_file(template, "", ACCESS_WRITE);
	}
	return rc;
}

int mkostemp(char *template, int flags)
{
	int rc;

	WRAP(s_mkostemp, "mkostemp");
	rc = s_mkostemp(template, flags);
	if(rc != -1) {
		handle_file(template, "", ACCESS_WRITE);
	}
	return rc;
}

int unlink(const char *pathname)
{
	int rc;

	WRAP(s_unlink, "unlink");
	rc = s_unlink(pathname);
	if(rc == 0)
		handle_file(pathname, "", ACCESS_UNLINK);
	return rc;
}

int unlinkat(int dirfd, const char *pathname, int flags)
{
	int rc;

	WRAP(s_unlinkat, "unlinkat");
	rc = s_unlinkat(dirfd, pathname, flags);
	if(rc == 0) {
		if(dirfd == AT_FDCWD) {
			handle_file(pathname, "", ACCESS_UNLINK);
		} else {
			fprintf(stderr, "tup.ldpreload: Error - unlinkat() not supported unless dirfd == AT_FDCWD\n");
			return -1;
		}
	}
	return rc;
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
	int rc;

	WRAP(s_execve, "execve");
	handle_file(filename, "", ACCESS_READ);
	rc = s_execve(filename, argv, envp);
	return rc;
}

int execv(const char *path, char *const argv[])
{
	int rc;

	WRAP(s_execv, "execv");
	handle_file(path, "", ACCESS_READ);
	rc = s_execv(path, argv);
	return rc;
}

int execl(const char *path, const char *arg, ...)
{
	if(path) {}
	if(arg) {}
	fprintf(stderr, "tup error: execl() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

int execlp(const char *file, const char *arg, ...)
{
	if(file) {}
	if(arg) {}
	fprintf(stderr, "tup error: execlp() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

int execle(const char *file, const char *arg, ...)
{
	if(file) {}
	if(arg) {}
	fprintf(stderr, "tup error: execle() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

int execvp(const char *file, char *const argv[])
{
	int rc;
	const char *p;

	WRAP(s_execvp, "execvp");
	for(p = file; *p; p++) {
		if(*p == '/') {
			handle_file(file, "", ACCESS_READ);
			rc = s_execvp(file, argv);
			return rc;
		}
	}
	rc = s_execvp(file, argv);
	return rc;
}

int chdir(const char *path)
{
	int rc;
	WRAP(s_chdir, "chdir");
	rc = s_chdir(path);
	if(rc == 0) {
		handle_file(path, "", ACCESS_CHDIR);
	}
	return rc;
}

int fchdir(int fd)
{
	if(fd) {}
	fprintf(stderr, "tup error: fchdir() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

int __xstat(int vers, const char *name, struct stat *buf)
{
	int rc;
	WRAP(s_xstat, "__xstat");
	rc = s_xstat(vers, name, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(name, "", ACCESS_GHOST);
		}
	}
	return rc;
}

#if defined(__APPLE__)
int stat(const char *filename, struct stat *buf)
{
	int rc;
	WRAP(s_stat, "stat"__DARWIN_SUF_64_BIT_INO_T);
	rc = s_stat(filename, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(filename, "", ACCESS_GHOST);
		}
	}
	return rc;
}
#endif

int stat64(const char *filename, struct stat64 *buf)
{
	int rc;
	WRAP(s_stat64, "stat64");
	rc = s_stat64(filename, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(filename, "", ACCESS_GHOST);
		}
	}
	return rc;
}

int __xstat64(int __ver, __const char *__filename,
	      struct stat64 *__stat_buf)
{
	int rc;
	WRAP(s_xstat64, "__xstat64");
	rc = s_xstat64(__ver, __filename, __stat_buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(__filename, "", ACCESS_GHOST);
		}
	}
	return rc;
}

int __lxstat64(int vers, const char *path, struct stat64 *buf)
{
	int rc;
	WRAP(s_lxstat64, "__lxstat64");
	rc = s_lxstat64(vers, path, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(path, "", ACCESS_GHOST);
		}
	}
	return rc;
}

static void handle_file(const char *file, const char *file2, int at)
{
	if(ignore_file(file))
		return;
	tup_send_event(file, strlen(file), file2, strlen(file2), at);
}

static int ignore_file(const char *file)
{
	if(strncmp(file, "/tmp/", 5) == 0)
		return 1;
	if(strncmp(file, "/dev/", 5) == 0)
		return 1;
	return 0;
}
