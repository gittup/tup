#ifndef graph_h
#define graph_h

#include "list.h"
#include "tupid.h"

struct edge {
	struct edge *next;
	struct node *dest;
};

struct node {
	struct list_head list;
	struct list_head processing;
	struct edge *edges;
	tupid_t tupid;
	int incoming_count;
};

struct node *find_node(const tupid_t tupid);
struct node *create_node(const tupid_t tupid);
void remove_node(struct node *n);

int create_edge(struct node *n1, struct node *n2);
struct edge *remove_edge(struct edge *e);

void dump_graph(const char *filename);

#endif
