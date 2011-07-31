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

struct run_script_info {
	int pfd[2];
	FILE *input;
	pid_t pid;
};

int server_init(void);
int server_quit(void);
int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd,
		struct tup_entry *dtent);
int server_is_dead(void);
int server_parser_start(struct server *s);
int server_parser_stop(struct server *s);

int server_run_script(struct run_script_info *rsi, int dfd, const char *cmdline);
int server_script_get_next_rule(struct run_script_info *rsi, char *buf, int size);
int server_run_script_quit(struct run_script_info *rsi);
void server_run_script_fail(struct run_script_info *rsi);

#endif
