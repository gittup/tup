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
	int exited;
	int signalled;
	int exit_status;
	int exit_sig;
	void *internal;
};

int server_init(void);
int server_setup(struct server *s, const char *jobdir);
int server_quit(struct server *s, int tupfd);
int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd,
		struct tup_entry *dtent);
int server_is_dead(void);

#endif
