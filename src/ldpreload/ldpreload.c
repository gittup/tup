#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include "access_event.h"
#include "debug.h"
#include "tupid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <dlfcn.h>

#define HANDLE_FILE(f, at) handle_file(f, at, __func__);

static void handle_file(const char *file, int at, const char *func);
static void handle_rename_file(const char *old, const char *new);
static int ignore_file(const char *file);
static void ldpre_init(void) __attribute__((constructor));
static int sd;
static struct sockaddr_un addr;
static int my_pid;

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
	if(rc == 0) {
		DEBUGP("renamed %s to %s\n", old, new);
		handle_rename_file(old, new);
	}
	return rc;
}

static void handle_file(const char *file, int at, const char *funcname)
{
	struct access_event event;

	if(ignore_file(file))
		return;
	DEBUGP("send file '%s' mode %i from func %s\n", file, at, funcname);

	event.at = at;
	event.pid = my_pid;
	tupid_from_filename(event.tupid, file);
	sendto(sd, &event, sizeof(event), 0, (void*)&addr, sizeof(addr));
}

static void handle_rename_file(const char *old, const char *new)
{
	if(ignore_file(old) || ignore_file(new))
		return;

	HANDLE_FILE(old, ACCESS_RENAME_FROM);
	HANDLE_FILE(new, ACCESS_RENAME_TO);
}

static int ignore_file(const char *file)
{
	if(strncmp(file, "/tmp/", 5) == 0) {
		return 1;
	}
	if(file[0] == '/') {
		/* TODO always ignore global file? */
		return 1;
	}
	return 0;
}

static void ldpre_init(void)
{
	char *path;

	my_pid = getpid();
	if(getenv(TUP_DEBUG) != NULL) {
		debug_enable("tup_ldpreload.so");
	}

	path = getenv(SERVER_NAME);
	if(!path) {
		fprintf(stderr, "tup.ldpreload: Unable to get '%s' "
			"path from the environment.\n", SERVER_NAME);
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
