#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "debug.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_node(FILE *f, struct node *n);
static struct node *rbnode_search(struct rb_root *root, tupid_t tupid);
static int rbnode_insert(struct rb_root *root, struct node *data);

struct node *find_node(struct graph *g, tupid_t tupid)
{
	return rbnode_search(&g->tree, tupid);
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
	n->sym = dbn->sym;
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
	n->expanded = 0;
	n->parsing = 0;
	list_add_tail(&n->list, &g->node_list);

	if(rbnode_insert(&g->tree, n) < 0)
		return NULL;
	return n;
}

void remove_node(struct graph *g, struct node *n)
{
	if(n->edges) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tupid);
	}
	rb_erase(&n->rbn, &g->tree);
	/* TODO: block pool */
	free(n->name);
	free(n);
}

int create_edge(struct node *n1, struct node *n2, int style)
{
	struct edge *e;

	/* TODO: block pool */
	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;
	e->style = style;

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
	struct db_node dbn_root = {0, 0, "root", TUP_NODE_ROOT, -1, -1};

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);

	g->tree.rb_node = NULL;

	g->cur = g->root = create_node(g, &dbn_root);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
	return 0;
}

int destroy_graph(struct graph *g)
{
	if(g) {/* TODO */}
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
	fprintf(f, "tup%p [label=\"%s [%lli] (%i, %i)\",color=\"#%06x\"];\n",
		n, n->name, n->tupid, n->incoming_count, n->expanded, color);
	/* TODO: slist_for_each? */
	for(e=n->edges; e; e=e->next) {
		fprintf(f, "tup%p -> tup%p [dir=back];\n", e->dest, n);
	}
}

static struct node *rbnode_search(struct rb_root *root, tupid_t tupid)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct node *data = rb_entry(node, struct node, rbn);
		int result;

		result = tupid - data->tupid;

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

static int rbnode_insert(struct rb_root *root, struct node *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct node *this = rb_entry(*new, struct node, rbn);
		int result = data->tupid - this->tupid;

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return -1;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->rbn, parent, new);
	rb_insert_color(&data->rbn, root);

	return 0;
}
