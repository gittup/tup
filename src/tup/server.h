#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include "pel_group.h"
#include <stdio.h>
#include <pthread.h>

struct tup_entry;

struct server {
	struct file_info finfo;
	int id;
	int exited;
	int signalled;
	int exit_status;
	int exit_sig;

#ifdef _WIN32
	/* TODO: Unify servers */
	int udp_port;
	int sd[2];
	pthread_t tid;
	tupid_t dt;
#endif
};

int server_init(void);
int server_quit(void);
int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd,
		struct tup_entry *dtent);
int server_is_dead(void);

#endif
