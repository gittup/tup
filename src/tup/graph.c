#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "debug.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_node(FILE *f, struct node *n);

int find_node(struct graph *g, tupid_t tupid, struct node **n)
{
	return memdb_find(&g->memdb, tupid, n);
}

struct node *create_node(struct graph *g, struct db_node *dbn)
{
	struct node *n;

	n = malloc(sizeof *n);
	if(!n) {
		perror("malloc");
		return NULL;
	}
	n->edges = NULL;
	n->tupid = dbn->tupid;
	n->dt = dbn->dt;
	n->incoming_count = 0;
	n->name = strdup(dbn->name);
	if(!n->name) {
		perror("strdup");
		return NULL;
	}
	n->state = STATE_INITIALIZED;
	n->type = dbn->type;
	n->flags = tup_db_get_node_flags(dbn->tupid);
	n->already_used = 0;
	list_add(&n->list, &g->plist);

	if(n->type == g->count_flags && ! (n->flags & TUP_FLAGS_DELETE))
		g->num_nodes++;

	if(memdb_add(&g->memdb, n->tupid, n) < 0)
		return NULL;
	return n;
}

void remove_node(struct graph *g, struct node *n)
{
	if(n->edges) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tupid);
	}
	memdb_remove(&g->memdb, n->tupid);
	/* TODO: block pool */
	free(n->name);
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

	/* TODO: slist add? */
	e->next = n1->edges;
	n1->edges = e;

	n2->incoming_count++;
	return 0;
}

struct edge *remove_edge(struct edge *e)
{
	struct edge *tmp;
	tmp = e->next;
	e->dest->incoming_count--;
	/* TODO: block pool */
	free(e);
	return tmp;
}

int create_graph(struct graph *g, int count_flags)
{
	struct db_node dbn_root = {0, 0, "root", TUP_NODE_ROOT};

	if(sizeof(struct node *) != 4) {
		fprintf(stderr, "Error: sizeof node pointer is not 32 bits (size = %i bytes). This needs to be fixed.\n", sizeof(struct node *));
		return -1;
	}

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);

	if(memdb_init(&g->memdb) < 0)
		return -1;

	g->cur = g->root = create_node(g, &dbn_root);
	if(!g->root)
		return -1;
	list_move(&g->root->list, &g->node_list);
	g->num_nodes = 0;
	g->count_flags = count_flags;
	return 0;
}

int destroy_graph(struct graph *g)
{
	if(memdb_close(&g->memdb) < 0)
		return -1;
	return 0;
}

void dump_graph(const struct graph *g, const char *filename)
{
	static int count = 0;
	struct node *n;
	char *realfile;
	FILE *f;

	if(asprintf(&realfile, filename, getpid(), count) < 0) {
		perror("asprintf");
		return;
	}
	fprintf(stderr, "Dumping graph '%s'\n", realfile);
	count++;
	f = fopen(realfile, "w");
	if(!f) {
		perror(realfile);
		return;
	}
	fprintf(f, "digraph G {\n");
	list_for_each_entry(n, &g->node_list, list) {
		dump_node(f, n);
	}
	list_for_each_entry(n, &g->plist, list) {
		dump_node(f, n);
	}
	fprintf(f, "}\n");
	fclose(f);
}

static void dump_node(FILE *f, struct node *n)
{
	struct edge *e;
	int color = 0;
	if(n->flags & TUP_FLAGS_CREATE)
		color |= 0x00bb00;
	if(n->flags & TUP_FLAGS_DELETE)
		color |= 0xff0000;
	if(n->flags & TUP_FLAGS_MODIFY)
		color |= 0x0000ff;
	fprintf(f, "tup%lli [label=\"%s (%i)\",color=\"#%06x\"];\n",
		n->tupid, n->name, n->incoming_count, color);
	/* TODO: slist_for_each? */
	for(e=n->edges; e; e=e->next) {
		fprintf(f, "tup%lli -> tup%lli [dir=back];\n",
			e->dest->tupid, n->tupid);
	}
}
