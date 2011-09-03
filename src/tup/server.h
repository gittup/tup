#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include "pel_group.h"
#include <pthread.h>

struct rb_root;
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
	tupid_t oldid;

#ifdef _WIN32
	/* TODO: Unify servers */
	int udp_port;
	int sd[2];
	pthread_t tid;
	tupid_t dt;
#endif
};

enum server_mode {
	SERVER_PARSER_MODE,
	SERVER_UPDATER_MODE,
};

int server_pre_init(void);
int server_post_exit(void);
int server_init(enum server_mode mode, struct rb_root *delete_tree);
int server_quit(void);
int server_exec(struct server *s, int dfd, const char *cmd,
		struct tup_entry *dtent);
int server_is_dead(void);
int server_parser_start(struct server *s);
int server_parser_stop(struct server *s);

int server_run_script(int dfd, const char *cmdline, char **rules);

#endif
