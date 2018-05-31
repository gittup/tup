/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * ldpreload - Shared library shim for file accesses.
 *
 * Copyright (c) 2008-2018  Mike Shal <marfey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

#define _GNU_SOURCE
#include "tup/access_event.h"
#include "tup/flock.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

int __xstat(int vers, const char *name, struct stat *buf);
int stat64(const char *filename, struct stat64 *buf);
int __xstat64(int __ver, __const char *__filename,
	      struct stat64 *__stat_buf);
int __lxstat64(int vers, const char *path, struct stat64 *buf);
char *__realpath_chk(const char *path, char *resolved_path, size_t resolvedlen);

static char cwd[PATH_MAX];
static int cwdlen = -1;
static void handle_file(const char *file, const char *file2, int at);
static void handle_file_dirfd(int dirfd, const char *file, const char *file2, int at);
static int ignore_file(const char *file);
static int update_cwd(void);

static int (*s_open)(const char *, int, ...);
static int (*s_open64)(const char *, int, ...);
static FILE *(*s_fopen)(const char *, const char *);
static FILE *(*s_fopen64)(const char *, const char *);
static FILE *(*s_freopen)(const char *, const char *, FILE *);
static int (*s_creat)(const char *, mode_t);
static int (*s_symlink)(const char *, const char *);
static int (*s_symlinkat)(const char *, int, const char *);
static ssize_t (*s_readlink)(const char *, char *, size_t);
static char *(*s_realpath)(const char *, char *);
static char *(*s_realpath_chk)(const char *, char *, size_t);
static int (*s_rename)(const char*, const char*);
static int (*s_renameat)(int, const char*, int, const char*);
static int (*s_mkstemp)(char *template);
static int (*s_mkostemp)(char *template, int flags);
static int (*s_remove)(const char *);
static int (*s_unlink)(const char*);
static int (*s_unlinkat)(int, const char*, int);
static int (*s_execve)(const char *filename, char *const argv[],
		       char *const envp[]);
static int (*s_execv)(const char *path, char *const argv[]);
static int (*s_execvp)(const char *file, char *const argv[]);
static int (*s_chdir)(const char *path);
static int (*s_fchdir)(int fd);
static int (*s_xstat)(int vers, const char *name, struct stat *buf);
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

#define VWRAP(ptr, name, version) \
	if(!ptr) { \
		ptr = dlvsym(RTLD_NEXT, name, version); \
		if(!ptr) { \
			fprintf(stderr, "tup.ldpreload: Unable to wrap '%s' version '%s'\n", \
				name, version); \
			exit(1); \
		} \
	}

static int errored = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int depfd = -1;

static void init_fd(void) __attribute__((constructor));
static void init_fd(void)
{
	const char *depfile;
	depfile = getenv(TUP_DEPFILE);
	if(!depfile) {
		fprintf(stderr, "tup error: Unable to find dependency filename in the TUP_DEPFILE environment variable.\n");
		goto out_error;
	}
	WRAP(s_open, "open");
	if(depfd < 0) {
		depfd = s_open(depfile, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0666);
		if(depfd < 0) {
			perror(depfile);
			fprintf(stderr, "tup error: Unable to write dependencies to a temporary file.\n");
			goto out_error;
		}
	}
	return;

out_error:
	errored = 1;
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
			handle_file(pathname, "", ACCESS_READ);
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
			handle_file(pathname, "", ACCESS_READ);
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
			handle_file(path, "", ACCESS_READ);
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
			handle_file(path, "", ACCESS_READ);
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
			handle_file(path, "", ACCESS_READ);
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
		handle_file(newpath, "", ACCESS_WRITE);
	return rc;
}

int symlinkat(const char *target, int newdirfd, const char *linkpath)
{
	int rc;
	WRAP(s_symlinkat, "symlinkat");
	rc = s_symlinkat(target, newdirfd, linkpath);
	if(rc == 0) {
		handle_file_dirfd(newdirfd, linkpath, "", ACCESS_WRITE);
	}
	return rc;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
	ssize_t rc;
	/* Force ENOENT for /etc/malloc.conf, which is used by jemalloc.
	 *
	 * The jemalloc library wraps malloc(), calloc(), etc, and calls
	 * readlink on /etc/malloc.conf in its static initializer. This calls
	 * into our hook, which means we try to call dlsym() to get the real
	 * readlink from libc.so. Unfortunately, dlsym() calls calloc() to
	 * allocate space for thread-specific variables, which jemalloc picks
	 * up and tries to re-initialize itself, and then deadlocks since the
	 * original initialization isn't complete yet. I haven't found aonther
	 * way to work around this since both our shared library shim and
	 * jemalloc have their symbols mapped into the process before the
	 * static initializer is called, and I don't think we can get the real
	 * readlink symbol without allocating memory.
	 */
	if(strcmp(pathname, "/etc/malloc.conf") == 0) {
		errno = ENOENT;
		return -1;
	}
	WRAP(s_readlink, "readlink");
	rc = s_readlink(pathname, buf, bufsiz);
	handle_file(pathname, "", ACCESS_READ);
	return rc;
}

char *realpath(const char *path, char *resolved_path)
{
	VWRAP(s_realpath, "realpath", "GLIBC_2.3");
	handle_file(path, "", ACCESS_READ);
	return s_realpath(path, resolved_path);
}

char *__realpath_chk(const char *path, char *resolved_path, size_t resolvedlen)
{
	WRAP(s_realpath_chk, "__realpath_chk");
	handle_file(path, "", ACCESS_READ);
	return s_realpath_chk(path, resolved_path, resolvedlen);
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

int renameat(int oldfd, const char *old, int newfd, const char *new)
{
	int rc;

	/* This shouldn't be too hard to implement, but at the moment we only
	 * need renameat() with AT_FDCWD for 'ln -s' on arch and fedora.
	 */
	if(oldfd != AT_FDCWD || newfd != AT_FDCWD) {
		fprintf(stderr, "tup error: renameat() with fd != AT_FDCWD is not yet supported.\n");
		errno = ENOSYS;
		return -1;
	}
	WRAP(s_renameat, "renameat");
	rc = s_renameat(oldfd, old, newfd, new);
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

int remove(const char *pathname)
{
	int rc;

	WRAP(s_remove, "remove");
	rc = s_remove(pathname);
	if(rc == 0)
		handle_file(pathname, "", ACCESS_UNLINK);
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
		handle_file_dirfd(dirfd, pathname, "", ACCESS_UNLINK);
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
		if(update_cwd() < 0)
			return -1;
	}
	return rc;
}

int fchdir(int fd)
{
	int rc;
	WRAP(s_fchdir, "fchdir");
	rc = s_fchdir(fd);
	if(rc == 0) {
		if(update_cwd() < 0)
			return -1;
	}
	return rc;
}

int __xstat(int vers, const char *name, struct stat *buf)
{
	int rc;
	WRAP(s_xstat, "__xstat");
	rc = s_xstat(vers, name, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(name, "", ACCESS_READ);
		}
	}
	return rc;
}

int stat64(const char *filename, struct stat64 *buf)
{
	int rc;
	WRAP(s_stat64, "stat64");
	rc = s_stat64(filename, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(filename, "", ACCESS_READ);
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
			handle_file(__filename, "", ACCESS_READ);
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
			handle_file(path, "", ACCESS_READ);
		}
	}
	return rc;
}

static int write_all(int fd, const void *data, int size)
{
	if(write(fd, data, size) != size) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write %i bytes to the dependency file.\n", size);
		return -1;
	}
	return 0;
}

static void handle_file_locked(const char *dirname, int dirlen, const char *file, const char *file2, int at)
{
	struct access_event event;
	int len;
	int len2;

	if(errored)
		return;
	if(ignore_file(file))
		return;

	if(tup_flock(depfd) < 0) {
		fprintf(stderr, "tup error: Unable to lock dependency file for writing [%i]: (file event = %s)\n", depfd, file);
		goto out_error;
	}
	len = strlen(file);
	len2 = strlen(file2);
	event.at = at;
	event.len = len;
	event.len2 = len2;
	if(!is_full_path(file))
		event.len += dirlen + 1;
	if(file2[0] && !is_full_path(file2))
		event.len2 += dirlen + 1;
	if(write_all(depfd, &event, sizeof(event)) < 0)
		goto out_error;
	if(!is_full_path(file)) {
		if(write_all(depfd, dirname, dirlen) < 0)
			goto out_error;
		if(write_all(depfd, "/", 1) < 0)
			goto out_error;
	}
	if(write_all(depfd, file, len + 1) < 0)
		goto out_error;
	if(file2[0] && !is_full_path(file2)) {
		if(write_all(depfd, dirname, dirlen) < 0)
			goto out_error;
		if(write_all(depfd, "/", 1) < 0)
			goto out_error;
	}
	if(write_all(depfd, file2, len2 + 1) < 0)
		goto out_error;
	if(tup_unflock(depfd) < 0) {
		fprintf(stderr, "tup error: Unable to unlock dependency file.\n");
		goto out_error;
	}
	return;

out_error:
	errored = 1;
}

static void handle_file(const char *file, const char *file2, int at)
{
	pthread_mutex_lock(&mutex);
	if(cwdlen < 0)
		update_cwd();
	handle_file_locked(cwd, cwdlen, file, file2, at);
	pthread_mutex_unlock(&mutex);
}

static void handle_file_dirfd(int dirfd, const char *file, const char *file2, int at)
{
	int dirlen;
	char procbuf[PATH_MAX];
	char dirname[PATH_MAX];

	if(dirfd == AT_FDCWD) {
		handle_file(file, file2, at);
		return;
	}
	snprintf(procbuf, sizeof(procbuf), "/proc/self/fd/%i", dirfd);
	WRAP(s_readlink, "readlink");
	dirlen = s_readlink(procbuf, dirname, sizeof(dirname));
	if(dirlen < 0) {
		perror(procbuf);
		fprintf(stderr, "tup.ldpreload: Error reading file descriptor symlink from /proc\n");
		errored = 1;
		return;
	}
	dirname[dirlen] = 0;
	pthread_mutex_lock(&mutex);
	handle_file_locked(dirname, dirlen, file, file2, at);
	pthread_mutex_unlock(&mutex);
}

static int ignore_file(const char *file)
{
	if(strncmp(file, "/dev/", 5) == 0)
		return 1;
	if(strncmp(file, "/proc/", 6) == 0)
		return 1;
	return 0;
}

static int update_cwd(void)
{
	if(getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd");
		return -1;
	}
	cwdlen = strlen(cwd);
	return 0;
}
