#ifndef tup_graph_h
#define tup_graph_h

#include "linux/list.h"
#include "db.h"
#include "tupid_tree.h"

struct edge {
	struct edge *next;
	struct node *dest;
	int style;
};

struct tup_entry;

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2

struct node {
	struct list_head list;
	struct edge *edges;
	int incoming_count;
	struct tupid_tree tnode;
	struct tup_entry *tent;

	char state;
	char already_used;
	char expanded;
	char parsing;
};

struct graph {
	struct list_head node_list;
	struct list_head plist;
	struct node *root;
	struct node *cur;
	int num_nodes;
	struct rb_root tree;
	int count_flags;
	struct rb_root delete_tree;
	int delete_count;
};

struct node *find_node(struct graph *g, tupid_t tupid);
struct node *create_node(struct graph *g, struct tup_entry *tent);
void remove_node(struct graph *g, struct node *n);

int create_edge(struct node *n1, struct node *n2, int style);
struct edge *remove_edge(struct edge *e);

int create_graph(struct graph *g, int count_flags);
int destroy_graph(struct graph *g);
int nodes_are_connected(struct tup_entry *src, struct list_head *dest_list,
			int *connected);
void dump_graph(const struct graph *g, const char *filename);

#endif
