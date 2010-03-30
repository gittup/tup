#include "server.h"
#include "file.h"
#include "debug.h"
#include "getexecwd.h"
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>

static void *message_thread(void *arg);

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
#ifdef __APPLE__
	setenv("DYLD_FORCE_FLAT_NAMESPACE", "", 1);
	setenv("DYLD_INSERT_LIBRARIES", ldpreload_path, 1);
#else
	setenv("LD_PRELOAD", ldpreload_path, 1);
#endif
}

int start_server(struct server *s)
{
	if(socketpair(AF_UNIX, SOCK_DGRAM, 0, s->sd) < 0) {
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
	enum access_type at = ACCESS_STOP_SERVER;
	int rc;

	rc = send(s->sd[1], &at, sizeof(at), 0);
	if(rc != sizeof(at)) {
		perror("write");
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
	struct access_event *event;
	char *filename;
	char *file2;
	int rc;
	struct server *s = arg;

	event = (struct access_event*)s->msgbuf;
	filename = &s->msgbuf[sizeof(*event)];
	while((rc = recv(s->sd[0], s->msgbuf, sizeof(s->msgbuf), 0)) > 0) {
		int expected;

		if(event->at == ACCESS_STOP_SERVER)
			break;
		if(!event->len)
			continue;

		expected = sizeof(*event) + event->len + event->len2;
		if(rc != expected) {
			fprintf(stderr, "Error: received %i bytes, expecting %i bytes.\n", rc, expected);
			return (void*)-1;
		}

		file2 = &s->msgbuf[sizeof(*event) + event->len];

		if(handle_file(event->at, filename, file2, &s->finfo) < 0) {
			return (void*)-1;
		}
		/* Oh noes! An electric eel! */
		;
	}
	if(rc < 0) {
		perror("recv");
		return (void*)-1;
	}
	return NULL;
}
