/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2013  Mike Shal <marfey@gmail.com>
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

#ifndef tup_parser_h
#define tup_parser_h

#include "tupid_tree.h"
#include "string_tree.h"
#include "timespan.h"
#include "vardb.h"

struct variant;
struct tup_entry;
struct graph;
struct parser_server;
struct lua_State;

struct tupfile {
	tupid_t tupid;
	struct variant *variant;
	struct tup_entry *curtent;
	struct tup_entry *srctent;
	int cur_dfd;
	int root_fd;
	int refactoring;
	struct graph *g;
	struct vardb vdb;
	struct node_vardb node_db;
	struct tupid_entries cmd_root;
	struct tupid_entries env_root;
	struct string_entries bang_root;
	struct tupid_entries input_root;
	struct string_entries chain_root;
	struct tupid_entries refactoring_cmd_delete_root;
	FILE *f;
	struct parser_server *ps;
	struct timespan ts;
	char ign;
	char circular_dep_error;
	struct lua_State *ls;
};

struct name_list_entry {
	TAILQ_ENTRY(name_list_entry) list;
	char *path;
	char *base;
	int len;
	int extlesslen;
	int baselen;
	int extlessbaselen;
	int dirlen;
	struct tup_entry *tent;
};
TAILQ_HEAD(name_list_entry_head, name_list_entry);

struct name_list {
	struct name_list_entry_head entries;
	int num_entries;
	int totlen;
	int basetotlen;
	int extlessbasetotlen;
};

struct rule {
	int foreach;
	char *input_pattern;
	char *output_pattern;
	struct bin *bin;
	const char *command;
	int command_len;
	struct name_list inputs;
	struct name_list order_only_inputs;
	struct name_list bang_oo_inputs;
	char *bang_extra_outputs;
	int empty_input;
	int line_number;
	struct name_list *output_nl;
};

struct bin_head;
struct path_list_head;

int execute_rule(struct tupfile *tf, struct rule *r, struct bin_head *bl);
int parse_dependent_tupfiles(struct path_list_head *plist, struct tupfile *tf);
int export(struct tupfile *tf, const char *cmdline);

struct node;
struct graph;
struct timespan;

void parser_debug_run(void);
int parse(struct node *n, struct graph *g, struct timespan *ts, int refactoring);

#endif
