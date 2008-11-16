#ifndef tup_graph_h
#define tup_graph_h

#include "tup/list.h"
#include "tup/tupid.h"
#include "tup/db.h"
#include <sqlite3.h>

struct edge {
	struct edge *next;
	struct node *dest;
};

#define STATE_INITIALIZED 0
#define STATE_PROCESSING 1
#define STATE_FINISHED 2

struct node {
	struct list_head list;
	struct edge *edges;
	tupid_t tupid;
	char *name;
	int incoming_count;

	char state;
	char type; /* One of TUP_NODE_* */
	char flags; /* One of TUP_FLAGS_* */
	char unused2;
};

struct graph {
	struct list_head node_list;
	struct list_head plist;
	struct node *root;
	struct node *cur;
	int num_nodes;
	sqlite3 *db;
};

struct node *find_node(const struct graph *g, tupid_t tupid);
struct node *create_node(struct graph *g, struct db_node *dbn);
void remove_node(struct graph *g, struct node *n);

int create_edge(struct node *n1, struct node *n2);
struct edge *remove_edge(struct edge *e);

int create_graph(struct graph *g);
void dump_graph(const struct graph *g, const char *filename);

#endif
