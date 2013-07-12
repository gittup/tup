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

#ifndef tup_graph_h
#define tup_graph_h

#include "bsd/queue.h"
#include "tupid_tree.h"
#include "db_types.h"
#include <time.h>
#include <stdio.h>

struct edge {
	LIST_ENTRY(edge) list;
	LIST_ENTRY(edge) destlist;
	struct node *dest;
	struct node *src;
	int style;
};
LIST_HEAD(edge_head, edge);

struct tup_entry;
struct tupid_entries;

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2
#define STATE_REMOVING 3

struct node {
	TAILQ_ENTRY(node) list;
	struct edge_head edges;
	struct edge_head incoming;
	struct tupid_tree tnode;
	struct tup_entry *tent;

	char state;
	char already_used;
	char expanded;
	char parsing;
};
TAILQ_HEAD(node_head, node);

struct graph {
	struct node_head node_list;
	struct node_head plist;
	struct node_head removing_list;
	struct node *root;
	struct node *cur;
	int num_nodes;
	struct tupid_entries node_root;
	enum TUP_NODE_TYPE count_flags;
	time_t total_mtime;
	struct tupid_entries gen_delete_root;
	int gen_delete_count;
	struct tupid_entries cmd_delete_root;
	int cmd_delete_count;
	struct tupid_entries normal_dir_root;
	struct tupid_entries parse_gitignore_root;
	int style;
};

struct node *find_node(struct graph *g, tupid_t tupid);
struct node *create_node(struct graph *g, struct tup_entry *tent);
void remove_node(struct graph *g, struct node *n);

int create_edge(struct node *n1, struct node *n2, int style);
void remove_edge(struct edge *e);

int create_graph(struct graph *g, enum TUP_NODE_TYPE count_flags);
int destroy_graph(struct graph *g);
void save_graphs(struct graph *g);
int build_graph_cb(void *arg, struct tup_entry *tent);
int build_graph(struct graph *g);
int graph_empty(struct graph *g);
int add_graph_stickies(struct graph *g);
int prune_graph(struct graph *g, int argc, char **argv, int *num_pruned);
void trim_graph(struct graph *g);
void save_graph(FILE *err, struct graph *g, const char *filename);
void dump_graph(struct graph *g, FILE *f, int show_dirs, int show_env, int show_ghosts);

int group_need_circ_check(void);
int add_group_circ_check(struct tup_entry *tent);
int group_circ_check(void);

#endif
