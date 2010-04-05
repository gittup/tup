#include "server.h"
#include "file.h"
#include "debug.h"
#include "getexecwd.h"
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>

static void *message_thread(void *arg);
static int recvall(int sd, void *buf, size_t len);

static char ldpreload_path[PATH_MAX];

int server_init(void)
{
	if(snprintf(ldpreload_path, sizeof(ldpreload_path),
		    "%s/tup-ldpreload.so",
		    getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for tup-ldpreload.so library is "
			"too long.\n");
		return -1;
	}
	return 0;
}

void server_setenv(struct server *s, int vardict_fd)
{
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%i", s->sd[1]);
	fd_name[31] = 0;
	setenv(TUP_SERVER_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", vardict_fd);
	fd_name[31] = 0;
	setenv(TUP_VARDICT_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", s->lockfd);
	fd_name[31] = 0;
	setenv(TUP_LOCK_NAME, fd_name, 1);
#ifdef __APPLE__
	setenv("DYLD_FORCE_FLAT_NAMESPACE", "", 1);
	setenv("DYLD_INSERT_LIBRARIES", ldpreload_path, 1);
#else
	setenv("LD_PRELOAD", ldpreload_path, 1);
#endif
}

int start_server(struct server *s)
{
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, s->sd) < 0) {
		perror("socketpair");
		return -1;
	}

	init_file_info(&s->finfo);

	if(pthread_create(&s->tid, NULL, message_thread, s) < 0) {
		perror("pthread_create");
		close(s->sd[0]);
		close(s->sd[1]);
		return -1;
	}

	return 0;
}

int stop_server(struct server *s)
{
	void *retval = NULL;
	struct access_event e;
	int rc;

	memset(&e, 0, sizeof(e));
	e.at = ACCESS_STOP_SERVER;

	rc = send(s->sd[1], &e, sizeof(e), 0);
	if(rc != sizeof(e)) {
		perror("send");
		return -1;
	}
	pthread_join(s->tid, &retval);
	close(s->sd[0]);
	close(s->sd[1]);

	if(retval == NULL)
		return 0;
	return -1;
}

static void *message_thread(void *arg)
{
	struct access_event event;
	struct server *s = arg;

	while(recvall(s->sd[0], &event, sizeof(event)) == 0) {
		if(event.at == ACCESS_STOP_SERVER)
			break;
		if(!event.len)
			continue;

		if(event.len >= (signed)sizeof(s->file1) - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event.len);
			return (void*)-1;
		}
		if(event.len2 >= (signed)sizeof(s->file2) - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event.len2);
			return (void*)-1;
		}

		if(recvall(s->sd[0], s->file1, event.len) < 0) {
			fprintf(stderr, "Error: Did not recv all of file1 in access event.\n");
			return (void*)-1;
		}
		if(recvall(s->sd[0], s->file2, event.len2) < 0) {
			fprintf(stderr, "Error: Did not recv all of file2 in access event.\n");
			return (void*)-1;
		}

		s->file1[event.len] = 0;
		s->file2[event.len2] = 0;

		if(handle_file(event.at, s->file1, s->file2, &s->finfo) < 0) {
			return (void*)-1;
		}
		/* Oh noes! An electric eel! */
		;
	}
	return NULL;
}

static int recvall(int sd, void *buf, size_t len)
{
	size_t recvd = 0;
	char *cur = buf;

	while(recvd < len) {
		int rc;
		rc = recv(sd, cur + recvd, len - recvd, 0);
		if(rc < 0) {
			perror("recv");
			return -1;
		}
		if(rc == 0)
			return -1;
		recvd += rc;
	}
	return 0;
}
