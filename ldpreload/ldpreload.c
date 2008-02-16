#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/socket.h>
#define __USE_GNU
#include <dlfcn.h>
#include "access_event.h"

#define HANDLE_FILE(f, rw) handle_file(f, rw, __func__);

static void handle_file(const char *file, int rw, const char *funcname);
static void ldpre_init(void) __attribute__((constructor));
static int sd;
static struct sockaddr_un addr;

/* Buffer used to send the file access data. It is a large buffer (since the
 * paths can be MAXPATHLEN large), so we only create one of them and protect
 * its use with the lock below.
 */
static struct access_event send_event;

/* Lock protects use of the send_event buffer */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

static void handle_file(const char *file, int rw, const char *funcname)
{
	if(strncmp(file, "/tmp/", 5) == 0) {
		return;
	}
	pthread_mutex_lock(&lock);
	fprintf(stderr, "MARF: Received file '%s' mode %i from %s\n",
		file, rw, funcname);

	send_event.rw = rw;
	strcpy(send_event.filename, file);

	/* TODO: Fix size calculation? */
	sendto(sd, &send_event, sizeof(send_event.rw) + strlen(file) + 1, 0,
	       (void*)&addr, sizeof(addr));
	pthread_mutex_unlock(&lock);
}

static void ldpre_init(void)
{
	char *path;

	path = getenv("tup_master");
	if(!path) {
		fprintf(stderr, "tup.ldpreload: Unable to get 'tup_master' "
			"path from the environment.\n");
		exit(1);
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	addr.sun_family = AF_UNIX;

	sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if(sd < 0) {
		perror("tup.ldpreload: socket");
		exit(1);
	}
}
