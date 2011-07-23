#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include "pel_group.h"
#include <pthread.h>

struct tup_entry;

struct server {
	struct file_info finfo;
	int id;
	int exited;
	int signalled;
	int exit_status;
	int exit_sig;

	/* For the parser */
	int root_fd;

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
int server_parser_start(struct tup_entry *tent, struct server *s);
int server_parser_stop(struct server *s);

#endif
