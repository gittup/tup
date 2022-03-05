/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2022  Mike Shal <marfey@gmail.com>
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

#include "file.h"
#include "bsd/queue.h"
#include "string_tree.h"
#include <pthread.h>

struct tent_entries;
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
	pthread_mutex_t *error_mutex;
};

struct parser_directory {
	/* parser_server gets one of these for each directory that is preloaded with files. */
	struct string_tree st;
	struct string_entries files;
};

struct parser_server {
	struct server s;
	int root_fd;
	struct parser_server *oldps;
	struct string_entries directories;
	pthread_mutex_t lock;
};

enum server_mode {
	SERVER_CONFIG_MODE,
	SERVER_PARSER_MODE,
	SERVER_UPDATER_MODE,
};

int server_pre_init(void);
int server_post_exit(void);
int server_init(enum server_mode mode);
int server_quit(void);
int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent, int need_namespacing, int run_in_bash);
int server_postexec(struct server *s);
int server_unlink(void);
int server_is_dead(void);
int server_parser_start(struct parser_server *ps);
int server_parser_stop(struct parser_server *ps);

int server_run_script(FILE *f, tupid_t tupid, const char *cmdline,
		      struct tent_entries *env_root, char **rules);
int serverless_run_script(FILE *f, const char *cmdline,
		          struct tent_entries *env_root, char **rules);
int server_symlink(struct server *s, struct tup_entry *dtent, const char *target, int dfd, const char *linkpath, struct tup_entry *output_tent);

#endif
