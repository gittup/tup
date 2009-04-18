#ifndef tup_graph_h
#define tup_graph_h

#include "list.h"
#include "tupid.h"
#include "db.h"
#include "memdb.h"

struct edge {
	struct edge *next;
	struct node *dest;
	int style;
};

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2

struct node {
	struct list_head list;
	struct edge *edges;
	tupid_t tupid;
	tupid_t dt;
	tupid_t sym;
	char *name;
	int incoming_count;

	char state;
	char type; /* One of TUP_NODE_* */
	char flags; /* One of TUP_FLAGS_* */
	char already_used;
	char expanded;
	char parsing;
	char unused1;
	char unused2;
};

struct graph {
	struct list_head node_list;
	struct list_head plist;
	struct node *root;
	struct node *cur;
	int num_nodes;
	struct memdb memdb;
	int count_flags;
};

int find_node(struct graph *g, tupid_t tupid, struct node **n);
struct node *create_node(struct graph *g, struct db_node *dbn);
void remove_node(struct graph *g, struct node *n);

int create_edge(struct node *n1, struct node *n2, int style);
struct edge *remove_edge(struct edge *e);

int create_graph(struct graph *g, int count_flags);
int destroy_graph(struct graph *g);
void dump_graph(const struct graph *g, const char *filename);

#endif
