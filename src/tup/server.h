#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "list.h"
#include "file.h"
#include <signal.h>
#include <sys/un.h>

struct server {
	struct list_head list;
	int sd;
	struct sockaddr_un addr;
	pthread_t tid;
	struct file_info finfo;
	char msgbuf[sizeof(struct access_event) + PATH_MAX];
	char cwd[PATH_MAX];
	char cname[PATH_MAX];
};

int server_init(void);
int start_server(struct server *s);
int stop_server(struct server *s);

#endif
