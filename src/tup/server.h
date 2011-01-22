#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include "fileio.h"
#include <pthread.h>

struct server {
	int sd[2];
	int lockfd;
	pthread_t tid;
	struct file_info finfo;
	tupid_t dt;
	struct pel_group pg;
	char file1[PATH_MAX];
	char file2[PATH_MAX];
};

int server_init(void);
void server_setenv(struct server *s, int vardict_fd);
int start_server(struct server *s, tupid_t dt);
int stop_server(struct server *s);

#endif
