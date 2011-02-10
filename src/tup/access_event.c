#include "access_event.h"
#include "flock.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>

static int sendall(int sd, const void *buf, size_t len);

void tup_send_event(const char *file, int len, const char *file2, int len2, int at)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static int tupsd;
	static int lockfd;
	struct access_event event;

	pthread_mutex_lock(&mutex);
	if(!file) {
		fprintf(stderr, "tup internal error: file can't be NUL\n");
		exit(1);
	}
	if(!file2) {
		fprintf(stderr, "tup internal error: file2 can't be NUL\n");
		exit(1);
	}
	if(!lockfd) {
		char *path;

		path = getenv(TUP_LOCK_NAME);
		if(!path) {
			fprintf(stderr, "tup: Unable to get '%s' "
				"path from the environment.\n", TUP_LOCK_NAME);
			exit(1);
		}
		lockfd = strtol(path, NULL, 0);
		if(lockfd <= 0) {
			fprintf(stderr, "tup: Unable to get valid file lock.\n");
			exit(1);
		}
	}

	if(!tupsd) {
		char *path;

		path = getenv(TUP_SERVER_NAME);
		if(!path) {
			fprintf(stderr, "tup: Unable to get '%s' "
				"path from the environment.\n", TUP_SERVER_NAME);
			exit(1);
		}
		tupsd = strtol(path, NULL, 0);
		if(tupsd <= 0) {
			fprintf(stderr, "tup: Unable to get valid socket descriptor.\n");
			exit(1);
		}
	}

	if(tup_flock(lockfd) < 0) {
		exit(1);
	}
	event.at = at;
	event.len = len;
	event.len2 = len2;
	if(sendall(tupsd, &event, sizeof(event)) < 0)
		exit(1);
	if(sendall(tupsd, file, event.len) < 0)
		exit(1);
	if(sendall(tupsd, file2, event.len2) < 0)
		exit(1);
	if(tup_unflock(lockfd) < 0)
		exit(1);
	pthread_mutex_unlock(&mutex);
}

static int sendall(int sd, const void *buf, size_t len)
{
	size_t sent = 0;
	const char *cur = buf;

	while(sent < len) {
		int rc;
		rc = send(sd, cur + sent, len - sent, 0);
		if(rc < 0) {
			perror("send");
			return -1;
		}
		sent += rc;
	}
	return 0;
}
