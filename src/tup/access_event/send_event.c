#include "tup/access_event.h"
#include "tup/flock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static void get_full_path(char *path, const char *file);

void tup_send_event(const char *file, int len, const char *file2, int len2, int at)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static int depsfd;
	struct access_event event;
	char path1[PATH_MAX];
	char path2[PATH_MAX];
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	if(len || len2) {/* unused */}

	pthread_mutex_lock(&mutex);
	if(!file) {
		fprintf(stderr, "tup internal error: file can't be NULL\n");
		exit(1);
	}
	if(!file2) {
		fprintf(stderr, "tup internal error: file2 can't be NULL\n");
		exit(1);
	}

	if(!depsfd) {
		char *path;

		path = getenv(TUP_SERVER_NAME);
		if(!path) {
			fprintf(stderr, "tup: Unable to get '%s' "
				"path from the environment.\n", TUP_SERVER_NAME);
			exit(1);
		}
		depsfd = open(path, O_WRONLY | O_APPEND);
		if(depsfd < 0) {
			perror(path);
			fprintf(stderr, "tup error: Unable to open dependency file.\n");
			exit(1);
		}
	}

	if(fcntl(depsfd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_WRLCK");
		exit(1);
	}

	if(strncmp(file, "@tup@/", 6) == 0) {
		strcpy(path1, file+6);
		path2[0] = 0;
		event.at = ACCESS_VAR;
	} else {
		get_full_path(path1, file);
		get_full_path(path2, file2);
		event.at = at;
	}
	event.len = strlen(path1);
	event.len2 = strlen(path2);
	if(write(depsfd, &event, sizeof(event)) < 0)
		exit(1);
	if(write(depsfd, path1, event.len + 1) < 0)
		exit(1);
	if(write(depsfd, path2, event.len2 + 1) < 0)
		exit(1);

	fl.l_type = F_UNLCK;
	if(fcntl(depsfd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_UNLCK");
		exit(1);
	}
	pthread_mutex_unlock(&mutex);
}

static void get_full_path(char *path, const char *file)
{
	if(file[0] == '/' || file[0] == 0) {
		/* Copy full paths or empty paths straight through */
		strncpy(path, file, PATH_MAX);
		path[PATH_MAX-1] = 0;
	} else {
		/* Otherwise prepend the cwd. */
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		if(snprintf(path, PATH_MAX, "%s/%s", cwd, file) >= PATH_MAX) {
			fprintf(stderr, "tup error: get_full_path sized incorrectly.\n");
			exit(1);
		}
	}
}
