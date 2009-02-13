#include "server.h"
#include "file.h"
#include "debug.h"
#include "getexecwd.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>

static void *message_thread(void *arg);
static void sighandler(int sig);

static char ldpreload_path[PATH_MAX];
static struct sigaction sigact;
static LIST_HEAD(server_list);
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

int server_init(void)
{
	if(snprintf(ldpreload_path, sizeof(ldpreload_path), "%s/ldpreload.so",
		    getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for ldpreload.so library is too "
			"long.\n");
		return -1;
	}

	sigact.sa_handler = sighandler;
	sigact.sa_flags = 0;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	return 0;
}

int start_server(struct server *s)
{
	s->sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if(s->sd < 0) {
		perror("socket");
		return -1;
	}
	s->addr.sun_family = AF_UNIX;
	snprintf(s->addr.sun_path, sizeof(s->addr.sun_path)-1, "/tmp/tup-%i.%i",
		 getpid(), s->sd);
	s->addr.sun_path[sizeof(s->addr.sun_path)-1] = 0;
	unlink(s->addr.sun_path);

	pthread_mutex_lock(&list_lock);
	list_add(&s->list, &server_list);
	pthread_mutex_unlock(&list_lock);

	if(bind(s->sd, (void*)&s->addr, sizeof(s->addr)) < 0) {
		perror("bind");
		close(s->sd);
		return -1;
	}

	init_file_info(&s->finfo);

	if(pthread_create(&s->tid, NULL, message_thread, s) < 0) {
		perror("pthread_create");
		close(s->sd);
		unlink(s->addr.sun_path);
	}

	setenv(SERVER_NAME, s->addr.sun_path, 1);
	setenv("LD_PRELOAD", ldpreload_path, 1);
	DEBUGP("started server '%s'\n", s->addr.sun_path);

	return 0;
}

int stop_server(struct server *s)
{
	void *retval = NULL;
	enum access_type at = ACCESS_STOP_SERVER;
	DEBUGP("stopping server '%s'\n", s->addr.sun_path);
	/* TODO: ok to reuse sd here? */
	sendto(s->sd, &at, sizeof(at), 0, (void*)&s->addr, sizeof(s->addr));
	pthread_join(s->tid, &retval);
	close(s->sd);
	unlink(s->addr.sun_path);
	unsetenv(SERVER_NAME);

	pthread_mutex_lock(&list_lock);
	list_del(&s->list);
	pthread_mutex_unlock(&list_lock);

	if(retval == NULL)
		return 0;
	return -1;
}

static void *message_thread(void *arg)
{
	struct access_event *event;
	char *filename;
	int rc;
	int dlen;
	struct server *s = arg;

	dlen = strlen(s->cwd);
	if(dlen >= (signed)sizeof(s->cwd) - 2) {
		fprintf(stderr, "Error: CWD[%s] is too large.\n", s->cwd);
		return (void*)-1;
	}
	s->cwd[dlen] = '/';
	s->cwd[dlen+1] = 0;

	event = (struct access_event*)s->msgbuf;
	filename = &s->msgbuf[sizeof(*event)];
	while((rc = recv(s->sd, s->msgbuf, sizeof(s->msgbuf), 0)) > 0) {
		int len;
		int expected;

		if(event->at == ACCESS_STOP_SERVER)
			break;
		if(!event->len)
			continue;

		expected = sizeof(*event) + event->len;
		if(rc != expected) {
			fprintf(stderr, "Error: received %i bytes, expecting %i bytes.\n", rc, expected);
			return (void*)-1;
		}

		if(filename[0] == '/') {
			len = canonicalize(filename, s->cname, sizeof(s->cname), NULL);
		} else {
			len = canonicalize2(s->cwd, filename, s->cname, sizeof(s->cname), NULL, "");
		}
		/* Skip the file if it's outside of our local tree */
		if(len < 0)
			continue;

		if(handle_file(event, s->cname, &s->finfo) < 0) {
			return (void*)-1;
		}
	}
	if(rc < 0) {
		perror("recv");
		return (void*)-1;
	}
	return NULL;
}

static void sighandler(int sig)
{
	struct server *s;
	/* Ensure the socket file is cleaned up if a signal is caught. */
	pthread_mutex_lock(&list_lock);
	list_for_each_entry(s, &server_list, list) {
		close(s->sd);
		unlink(s->addr.sun_path);
	}
	pthread_mutex_unlock(&list_lock);
	exit(sig);
}
