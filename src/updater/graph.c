#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LIST_HEAD(node_list);

struct node *find_node(const tupid_t tupid)
{
	struct node *n;

	/* TODO: Use hash */
	list_for_each_entry(n, &node_list, list) {
		if(memcmp(n->tupid, tupid, sizeof(n->tupid)) == 0)
			return n;
	}
	return NULL;
}

struct node *create_node(const tupid_t tupid)
{
	struct node *n;

	n = malloc(sizeof *n);
	if(!n) {
		perror("malloc");
		return NULL;
	}
	list_add(&n->list, &node_list);
	INIT_LIST_HEAD(&n->processing);
	INIT_LIST_HEAD(&n->edges);
	memcpy(n->tupid, tupid, sizeof(n->tupid));
	n->incoming_count = 0;
	n->visited = 0;
	return n;
}

void remove_node(struct node *n)
{
	list_del(&n->list);
	list_del(&n->processing);
	if(!list_empty(&n->edges)) {
		DEBUGP("Warning: Node %.*s still has edges.\n", 8, n->tupid);
	}
	/* TODO: block pool */
	free(n);
}

int create_edge(struct node *n1, struct node *n2)
{
	struct edge *e;

	/* TODO: block pool */
	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;
	list_add(&e->list, &n1->edges);
	n2->incoming_count++;
	return 0;
}

void remove_edge(struct edge *e)
{
	e->dest->incoming_count--;
	list_del(&e->list);
	/* TODO: block pool */
	free(e);
}

void dump_graph(const char *filename)
{
	static int count = 0;
	struct node *n;
	char *realfile;
	FILE *f;

	if(asprintf(&realfile, filename, count) < 0) {
		perror("asprintf");
		return;
	}
	count++;
	f = fopen(realfile, "w");
	if(!f) {
		perror(realfile);
		return;
	}
	fprintf(f, "digraph G {\n");
	list_for_each_entry(n, &node_list, list) {
		struct edge *e;
		fprintf(f, "tup%.*s [label=\"%.*s (%i)\"];\n",
			sizeof(tupid_t), n->tupid,
			8, n->tupid,
			n->incoming_count);
		list_for_each_entry(e, &n->edges, list) {
			fprintf(f, "tup%.*s -> tup%.*s [dir=back];\n",
				sizeof(tupid_t), e->dest->tupid,
				sizeof(tupid_t), n->tupid);
		}
	}
	fprintf(f, "}\n");
	fclose(f);
}
