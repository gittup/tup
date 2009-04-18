#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include <signal.h>
#include <sys/un.h>

struct server {
	int sd[2];
	pthread_t tid;
	pthread_mutex_t *db_mutex;
	tupid_t dt;
	struct file_info finfo;
	char msgbuf[sizeof(struct access_event) + PATH_MAX*2];
};

int server_init(void);
void server_setenv(struct server *s);
int start_server(struct server *s);
int stop_server(struct server *s);

#endif
