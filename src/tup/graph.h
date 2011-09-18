#ifndef tup_graph_h
#define tup_graph_h

#include "db.h"
#include "tupid_tree.h"

struct edge {
	LIST_ENTRY(edge) list;
	LIST_ENTRY(edge) destlist;
	struct node *dest;
	struct node *src;
	int style;
};
LIST_HEAD(edge_head, edge);

struct tup_entry;
struct tup_entry_head;

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2

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
	struct node *root;
	struct node *cur;
	int num_nodes;
	struct tupid_entries node_root;
	int count_flags;
	struct tupid_entries delete_root;
	int delete_count;
};

struct node *find_node(struct graph *g, tupid_t tupid);
struct node *create_node(struct graph *g, struct tup_entry *tent);
void remove_node(struct graph *g, struct node *n);

int create_edge(struct node *n1, struct node *n2, int style);
void remove_edge(struct edge *e);

int create_graph(struct graph *g, int count_flags);
int destroy_graph(struct graph *g);
int graph_empty(struct graph *g);
int nodes_are_connected(struct tup_entry *src, struct tup_entry_head *dest_head,
			int *connected);
int prune_graph(struct graph *g, int argc, char **argv, int *num_pruned);
void dump_graph(const struct graph *g, const char *filename);

#endif
