#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "entry.h"
#include "debug.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_node(FILE *f, struct node *n);
static struct tup_entry root_entry;
static char root_name[] = "root";

struct node *find_node(struct graph *g, tupid_t tupid)
{
	struct tupid_tree *tnode;
	tnode = tupid_tree_search(&g->tree, tupid);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct node, tnode);
}

struct node *create_node(struct graph *g, struct tup_entry *tent)
{
	struct node *n;

	n = malloc(sizeof *n);
	if(!n) {
		perror("malloc");
		return NULL;
	}
	n->edges = NULL;
	n->incoming_count = 0;
	n->tnode.tupid = tent->tnode.tupid;
	n->tent = tent;
	n->state = STATE_INITIALIZED;
	n->already_used = 0;
	n->expanded = 0;
	n->parsing = 0;
	list_add_tail(&n->list, &g->node_list);

	if(tupid_tree_insert(&g->tree, &n->tnode) < 0)
		return NULL;
	return n;
}

static void __remove_node(struct graph *g, struct node *n)
{
	list_del(&n->list);
	while(n->edges) {
		struct edge *tmp;
		tmp = n->edges->next;
		free(n->edges);
		n->edges = tmp;
	}
	remove_node(g, n);
}

void remove_node(struct graph *g, struct node *n)
{
	if(n->edges) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tnode.tupid);
	}
	rb_erase(&n->tnode.rbn, &g->tree);
	free(n);
}

int create_edge(struct node *n1, struct node *n2, int style)
{
	struct edge *e;

	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;
	e->style = style;

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
	free(e);
	return tmp;
}

int create_graph(struct graph *g, int count_flags)
{
	root_entry.tnode.tupid = 0;
	root_entry.dt = 0;
	root_entry.sym = -1;
	root_entry.parent = NULL;
	root_entry.symlink = NULL;
	root_entry.type = TUP_NODE_ROOT;
	root_entry.mtime = -1;
	root_entry.name.len = strlen(root_name);
	root_entry.name.s = root_name;
	root_entry.entries.rb_node = NULL;

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);
	g->delete_tree.rb_node = NULL;
	g->delete_count = 0;

	g->tree.rb_node = NULL;

	g->cur = g->root = create_node(g, &root_entry);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
	return 0;
}

int destroy_graph(struct graph *g)
{
	while(!list_empty(&g->plist)) {
		__remove_node(g, list_entry(g->plist.next, struct node, list));
	}
	while(!list_empty(&g->node_list)) {
		__remove_node(g, list_entry(g->node_list.next, struct node, list));
	}
	return 0;
}

static int add_file_cb(void *arg, struct tup_entry *tent, int style)
{
	struct graph *g = arg;
	struct node *n;
	int strongly_connected;

	n = find_node(g, tent->tnode.tupid);
	if(n != NULL)
		goto edge_create;
	n = create_node(g, tent);
	if(!n)
		return -1;

edge_create:
	if(n->state == STATE_PROCESSING) {
		/* A circular dependency is not guaranteed to trigger this,
		 * but it is easy to check before going through the graph.
		 */
		fprintf(stderr, "Error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			g->cur->tnode.tupid, tent->tnode.tupid);
		return -1;
	}
	strongly_connected = 0;
	/* Commands are always strongly connected to their outputs, since we
	 * restrict commands to make sure that they always write all of their
	 * outputs.
	 */
	if(g->cur->tent->type == TUP_NODE_CMD) {
		strongly_connected = 1;
	} else {
		if(style == (TUP_LINK_NORMAL | TUP_LINK_STICKY))
			strongly_connected = 1;
	}
	if(strongly_connected && n->expanded == 0) {
		if(n->tent->type == g->count_flags)
			g->num_nodes++;
		n->expanded = 1;
		list_move(&n->list, &g->plist);
	}

	if(create_edge(g->cur, n, style) < 0)
		return -1;
	return 0;
}

int nodes_are_connected(struct tup_entry *src, struct list_head *dest_list,
			int *connected)
{
	struct graph g;
	struct node *n;

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	n = create_node(&g, src);
	if(!n)
		return -1;
	if(create_edge(g.cur, n, TUP_LINK_NORMAL) < 0)
		return -1;
	n->expanded = 1;
	list_move(&n->list, &g.plist);

	*connected = 0;
	while(!list_empty(&g.plist)) {
		struct tup_entry *tent;
		n = list_entry(g.plist.next, struct node, list);

		list_for_each_entry(tent, dest_list, list) {
			if(tent == src)
				continue;
			if(tent == n->tent) {
				*connected = 1;
				goto out_cleanup;
			}
		}

		if(n->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", n->tnode.tupid);
			g.cur = n;
			if(tup_db_select_node_by_link(add_file_cb, &g, n->tnode.tupid) < 0)
				return -1;
			n->state = STATE_PROCESSING;
		} else if(n->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %lli\n", n->tnode.tupid);
			list_del(&n->list);
			list_add_tail(&n->list, &g.node_list);
			n->state = STATE_FINISHED;
		} else if(n->state == STATE_FINISHED) {
			fprintf(stderr, "tup internal error: STATE_FINISHED node %lli in plist\n", n->tnode.tupid);
			tup_db_print(stderr, n->tnode.tupid);
			return -1;
		}
	}

out_cleanup:
	if(destroy_graph(&g) < 0)
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
	int flags;

	flags = tup_db_get_node_flags(n->tnode.tupid);
	if(flags & TUP_FLAGS_CREATE)
		color |= 0x00bb00;
	if(flags & TUP_FLAGS_MODIFY)
		color |= 0x0000ff;
	fprintf(f, "tup%p [label=\"%s [%lli] (%i, %i)\",color=\"#%06x\"];\n",
		n, n->tent->name.s, n->tnode.tupid, n->incoming_count, n->expanded, color);
	for(e=n->edges; e; e=e->next) {
		fprintf(f, "tup%p -> tup%p [dir=back];\n", e->dest, n);
	}
}
