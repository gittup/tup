/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef server_h
#define server_h

#include "access_event.h"
#include "compat.h"
#include "file.h"
#include "pel_group.h"
#include <pthread.h>

struct tupid_entries;
struct tup_entry;
struct tup_env;

struct server {
	struct file_info finfo;
	int id;
	int exited;
	int signalled;
	int exit_status;
	int exit_sig;
	int output_fd;
	int error_fd;

	/* For the parser */
	int root_fd;
	tupid_t oldid;

#ifdef _WIN32
	/* TODO: Unify servers */
	int udp_port;
	int sd[2];
	pthread_t tid;
#endif
};

enum server_mode {
	SERVER_PARSER_MODE,
	SERVER_UPDATER_MODE,
};

int server_pre_init(void);
int server_post_exit(void);
int server_init(enum server_mode mode, struct tupid_entries *delete_root);
int server_quit(void);
int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent);
int server_postexec(struct server *s);
int server_is_dead(void);
int server_parser_start(struct server *s);
int server_parser_stop(struct server *s);

int server_run_script(tupid_t tupid, const char *cmdline,
		      struct tupid_entries *env_root, char **rules);

#endif
