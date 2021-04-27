/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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
#include "tent_tree.h"
#include "tent_list.h"
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

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2
#define STATE_REMOVING 3

enum transient_type {
	TRANSIENT_NONE,
	TRANSIENT_PROCESSING,
	TRANSIENT_DELETE,
};

enum graph_prune_type {
	GRAPH_PRUNE_GENERATED,
	GRAPH_PRUNE_ALL,
};

struct node {
	TAILQ_ENTRY(node) list;
	struct edge_head edges;
	struct edge_head incoming;
	struct tupid_tree tnode;
	struct tup_entry *tent;
	struct node_head *active_list;

	unsigned char state;
	unsigned char already_used;
	unsigned char expanded;
	unsigned char parsing;
	unsigned char marked;
	unsigned char skip;
	unsigned char transient;
};
TAILQ_HEAD(node_head, node);

struct graph {
	struct node_head node_list;
	struct node_head plist;
	struct node_head removing_list;
	struct tent_entries transient_root;
	struct node *root;
	struct node *cur;
	int num_nodes;
	struct tupid_entries node_root;
	enum TUP_NODE_TYPE count_flags;
	time_t total_mtime;
	struct tent_entries gen_delete_root;
	struct tent_entries save_root;
	struct tent_entries cmd_delete_root;
	struct tent_entries normal_dir_root;
	struct tent_entries parse_gitignore_root;
	int style;
};

struct node *find_node(struct graph *g, tupid_t tupid);
struct node *create_node(struct graph *g, struct tup_entry *tent);
void remove_node(struct graph *g, struct node *n);
int node_insert_tail(struct node_head *head, struct node *n);
int node_insert_head(struct node_head *head, struct node *n);
int node_remove_list(struct node_head *head, struct node *n);

int create_edge(struct node *n1, struct node *n2, int style);
void remove_edge(struct edge *e);

int create_graph(struct graph *g, enum TUP_NODE_TYPE count_flags);
int destroy_graph(struct graph *g);
void save_graphs(struct graph *g);
int build_graph_transient_cb(void *arg, struct tup_entry *tent);
int build_graph_non_transient_cb(void *arg, struct tup_entry *tent);
int build_graph_cb(void *arg, struct tup_entry *tent);
int build_graph_group_cb(void *arg, struct tup_entry *tent);
int build_graph(struct graph *g);
int graph_empty(struct graph *g);
int add_graph_stickies(struct graph *g);
int prune_graph(struct graph *g, int argc, char **argv, int *num_pruned,
		enum graph_prune_type gpt, int verbose);
int nodes_are_connected(struct tup_entry *src, struct tent_entries *valid_root,
			int *connected);
void trim_graph(struct graph *g);
void save_graph(FILE *err, struct graph *g, const char *filename);
void dump_graph(struct graph *g, FILE *f, int show_dirs, int combine);

int group_need_circ_check(void);
int add_group_circ_check(struct tup_entry *tent);
int group_circ_check(void);

#endif
