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
#include <sys/stat.h>

static void handle_file(const char *file, const char *file2, int at);
static int ignore_file(const char *file);

static int (*s_open)(const char *, int, ...);
static FILE *(*s_fopen)(const char *, const char *);
static FILE *(*s_freopen)(const char *, const char *, FILE *);
static int (*s_creat)(const char *, mode_t);
static int (*s_symlink)(const char *, const char *);
static int (*s_rename)(const char*, const char*);
static int (*s_mkstemp)(char *template);
static int (*s_unlink)(const char*);
static int (*s_execve)(const char *filename, char *const argv[],
		       char *const envp[]);
static int (*s_execv)(const char *path, char *const argv[]);
static int (*s_execvp)(const char *file, char *const argv[]);
static int (*s_stat)(const char *name, struct stat *buf);
static int (*s_stat64)(const char *name, struct stat64 *buf);

#define WRAP(ptr, name) \
	if(!ptr) { \
		ptr = dlsym(RTLD_NEXT, name); \
		if(!ptr) { \
			fprintf(stderr, "tup.ldpreload: Unable to wrap '%s'\n", \
				name); \
			exit(1); \
		} \
	}

int real_open(const char *pathname, int flags);
int real_open(const char *pathname, int flags)
{
	WRAP(s_open, "open");
	return s_open(pathname, flags, 0);
}

int open(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;
	int at = ACCESS_READ;

	fprintf(stderr, "OPEN: %s\n", pathname);
	WRAP(s_open, "open");
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	rc = s_open(pathname, flags, mode);

	if(flags&O_WRONLY || flags&O_RDWR)
		at = ACCESS_WRITE;
	handle_file(pathname, "", at);
	return rc;
}

FILE *fopen(const char *path, const char *mode)
{
	FILE *f;

	WRAP(s_fopen, "fopen");
	f = s_fopen(path, mode);
	handle_file(path, "", !(mode[0] == 'r'));
	return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
	FILE *f;

	WRAP(s_freopen, "freopen");
	f = s_freopen(path, mode, stream);
	handle_file(path, "", !(mode[0] == 'r'));
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
	handle_file(newpath, "", ACCESS_WRITE);
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

int unlink(const char *pathname)
{
	int rc;

	WRAP(s_unlink, "unlink");
	rc = s_unlink(pathname);
	if(rc == 0)
		handle_file(pathname, "", ACCESS_UNLINK);
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

int stat(const char *filename, struct stat *buf)
{
	int rc;
	WRAP(s_stat, "stat"__DARWIN_SUF_64_BIT_INO_T);
	rc = s_stat(filename, buf);
	/* If we stat directories here, the getcwd() in send_event.c will 
	 * cause an infinite loop, since apparently getcwd() does a stat.
	 */
	if(!S_ISDIR(buf->st_mode)) {
		handle_file(filename, "", ACCESS_READ);
	}
	return rc;
}

int stat64(const char *filename, struct stat64 *buf)
{
	int rc;
	WRAP(s_stat64, "stat64");
	rc = s_stat64(filename, buf);
	handle_file(filename, "", ACCESS_READ);
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
