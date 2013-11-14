#define _GNU_SOURCE
#include "tup/access_event.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static void handle_file(const char *file, const char *file2, int at);
static int ignore_file(const char *file);

static int tup_open(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;
	int at = ACCESS_READ;

	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	rc = open(pathname, flags, mode);

	if(flags&O_WRONLY || flags&O_RDWR)
		at = ACCESS_WRITE;
	handle_file(pathname, "", at);
	return rc;
}

static FILE *tup_fopen(const char *path, const char *mode)
{
	FILE *f;

	f = fopen(path, mode);
	handle_file(path, "", !(mode[0] == 'r'));
	return f;
}

static FILE *tup_freopen(const char *path, const char *mode, FILE *stream)
{
	FILE *f;

	f = freopen(path, mode, stream);
	handle_file(path, "", !(mode[0] == 'r'));
	return f;
}

static int tup_creat(const char *pathname, mode_t mode)
{
	int rc;

	rc = creat(pathname, mode);
	if(rc >= 0)
		handle_file(pathname, "", ACCESS_WRITE);
	return rc;
}

static int tup_access(const char *path, int amode)
{
	int rc;

	rc = access(path, amode);
	handle_file(path, "", ACCESS_READ);
	return rc;
}

static int tup_link(const char *path1, const char *path2)
{
	if(path1) {}
	if(path2) {}
	fprintf(stderr, "tup error: hard links are not supported.\n");
	errno = ENOSYS;
	return -1;
}

static int tup_symlink(const char *oldpath, const char *newpath)
{
	int rc;
	rc = symlink(oldpath, newpath);
	handle_file(newpath, "", ACCESS_WRITE);
	return rc;
}

static int tup_rename(const char *old, const char *new)
{
	int rc;

	rc = rename(old, new);
	if(rc == 0) {
		if(!ignore_file(old) && !ignore_file(new)) {
			handle_file(old, new, ACCESS_RENAME);
		}
	}
	return rc;
}

static int tup_mkstemp(char *template)
{
	int rc;

	rc = mkstemp(template);
	if(rc != -1) {
		handle_file(template, "", ACCESS_WRITE);
	}
	return rc;
}

static int tup_unlink(const char *pathname)
{
	int rc;

	rc = unlink(pathname);
	if(rc == 0)
		handle_file(pathname, "", ACCESS_UNLINK);
	return rc;
}

static int tup_execve(const char *filename, char *const argv[], char *const envp[])
{
	int rc;

	handle_file(filename, "", ACCESS_READ);
	rc = execve(filename, argv, envp);
	return rc;
}

static int tup_execv(const char *path, char *const argv[])
{
	int rc;

	handle_file(path, "", ACCESS_READ);
	rc = execv(path, argv);
	return rc;
}

static int tup_execl(const char *path, const char *arg, ...)
{
	if(path) {}
	if(arg) {}
	fprintf(stderr, "tup error: execl() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

static int tup_execlp(const char *file, const char *arg, ...)
{
	if(file) {}
	if(arg) {}
	fprintf(stderr, "tup error: execlp() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

static int tup_execle(const char *file, const char *arg, ...)
{
	if(file) {}
	if(arg) {}
	fprintf(stderr, "tup error: execle() is not supported.\n");
	errno = ENOSYS;
	return -1;
}

static int tup_execvp(const char *file, char *const argv[])
{
	int rc;
	const char *p;

	for(p = file; *p; p++) {
		if(*p == '/') {
			handle_file(file, "", ACCESS_READ);
			rc = execvp(file, argv);
			return rc;
		}
	}
	rc = execvp(file, argv);
	return rc;
}

static int tup_stat(const char *filename, struct stat *buf)
{
	int rc;
	rc = stat(filename, buf);
	/* If we stat directories here, the getcwd() in send_event.c will 
	 * cause an infinite loop, since apparently getcwd() does a stat.
	 */
	if(!S_ISDIR(buf->st_mode)) {
		handle_file(filename, "", ACCESS_READ);
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

struct interpose {
	void *new_func;
	void *orig_func;
};

static const struct interpose interposers[] __attribute__ ((section("__DATA, __interpose"))) __attribute__((used)) = {
	{(void*)tup_open, (void*)open},
	{(void*)tup_fopen, (void*)fopen},
	{(void*)tup_freopen, (void*)freopen},
	{(void*)tup_creat, (void*)creat},
	{(void*)tup_access, (void*)access},
	{(void*)tup_link, (void*)link},
	{(void*)tup_symlink, (void*)symlink},
	{(void*)tup_rename, (void*)rename},
	{(void*)tup_mkstemp, (void*)mkstemp},
	{(void*)tup_unlink, (void*)unlink},
	{(void*)tup_execve, (void*)execve},
	{(void*)tup_execv, (void*)execv},
	{(void*)tup_execl, (void*)execl},
	{(void*)tup_execlp, (void*)execlp},
	{(void*)tup_execle, (void*)execle},
	{(void*)tup_execvp, (void*)execvp},
	{(void*)tup_stat, (void*)stat},
};
