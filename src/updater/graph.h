#ifndef graph_h
#define graph_h

#include "list.h"
#include "tupid.h"

struct edge {
	struct edge *next;
	struct node *dest;
};

enum node_state {
	NODE_INITIALIZED,
	NODE_PROCESSING,
	NODE_FINISHED,
};

struct node {
	struct list_head list;
	struct edge *edges;
	tupid_t tupid;
	int incoming_count;
	int state;
};

struct graph {
	struct list_head node_list;
	struct list_head plist;
};

struct node *find_node(const struct graph *g, const tupid_t tupid);
struct node *create_node(const tupid_t tupid);
void remove_node(struct node *n);

int create_edge(struct node *n1, struct node *n2);
struct edge *remove_edge(struct edge *e);

void dump_graph(const struct graph *g, const char *filename);

#endif
