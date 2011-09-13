#ifdef TUP_GRAPH_DEBUGGING
#define _GNU_SOURCE /* TODO: For asprintf */
#endif
#include "graph.h"
#include "entry.h"
#include "debug.h"
#include "fileio.h"
#include "config.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	INIT_LIST_HEAD(&n->edges);
	INIT_LIST_HEAD(&n->incoming);
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

static void remove_node_internal(struct graph *g, struct node *n)
{
	list_del(&n->list);
	while(!list_empty(&n->edges)) {
		remove_edge(list_entry(n->edges.next, struct edge, list));
	}
	while(!list_empty(&n->incoming)) {
		remove_edge(list_entry(n->incoming.next, struct edge, destlist));
	}
	remove_node(g, n);
}

void remove_node(struct graph *g, struct node *n)
{
	if(!list_empty(&n->edges)) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tnode.tupid);
	}
	if(!list_empty(&n->incoming)) {
		DEBUGP("Warning: Node %lli still has incoming edges.\n", n->tnode.tupid);
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
	e->src = n1;
	e->style = style;

	list_add(&e->list, &n1->edges);
	list_add(&e->destlist, &n2->incoming);

	return 0;
}

void remove_edge(struct edge *e)
{
	list_del(&e->list);
	list_del(&e->destlist);
	free(e);
}

int create_graph(struct graph *g, int count_flags)
{
	root_entry.tnode.tupid = 0;
	root_entry.dt = 0;
	root_entry.parent = NULL;
	root_entry.type = TUP_NODE_ROOT;
	root_entry.mtime = -1;
	root_entry.name.len = strlen(root_name);
	root_entry.name.s = root_name;
	RB_INIT(&root_entry.entries);

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
		remove_node_internal(g, list_entry(g->plist.next, struct node, list));
	}
	while(!list_empty(&g->node_list)) {
		remove_node_internal(g, list_entry(g->node_list.next, struct node, list));
	}
	return 0;
}

int graph_empty(struct graph *g)
{
	if(g->node_list.prev == &g->root->list)
		return 1;
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

/* Marks the node and everything that links to it, on up to the root node.
 * Everything that is marked will stay in the PDAG; the rest are pruned.
 *
 * Note that this abuses the 'parsing' flag, since that flag is normally only
 * used in the parsing phase, and this is only used during the update phase.
 */
static void mark_nodes(struct node *n)
{
	struct edge *e;

	/* If we're already marked, no need to go any further. */
	if(n->parsing)
		return;

	n->parsing = 1;
	list_for_each_entry(e, &n->incoming, destlist) {
		struct node *mark = e->src;

		mark_nodes(mark);

		/* A command node must have all of its outputs marked, or we
		 * risk not unlinking all of its outputs in the updater before
		 * running it (t6055).
		 */
		if(mark->tent->type == TUP_NODE_CMD) {
			struct edge *e2;
			list_for_each_entry(e2, &mark->edges, list) {
				mark_nodes(e2->dest);
			}
		}
	}
}

static int prune_node(struct graph *g, struct node *n, int *num_pruned)
{
	if(n->tent->type == g->count_flags && n->expanded) {
		g->num_nodes--;
		(*num_pruned)++;

		if(n->tent->type != TUP_NODE_CMD) {
			fprintf(stderr, "tup internal error: node of type %i trying to add to the modify list in prune_graph\n", n->tent->type);
			return -1;
		}
		/* Mark all pruned commands as modify to make sure they are
		 * still executed on the next update (since we won't hit the
		 * logic that normally adds them in update_work())
		 */
		if(tup_db_add_modify_list(n->tent->tnode.tupid) < 0)
			return -1;
	}
	remove_node_internal(g, n);
	return 0;
}

int prune_graph(struct graph *g, int argc, char **argv, int *num_pruned)
{
	struct list_head *prune_list;
	int x;
	int dashdash = 0;

	*num_pruned = 0;

	prune_list = tup_entry_get_list();
	for(x=0; x<argc; x++) {
		struct tup_entry *tent;

		if(!dashdash) {
			if(strcmp(argv[x], "--") == 0) {
				dashdash = 1;
			}
			if(argv[x][0] == '-')
				continue;
		}
		tent = get_tent_dt(get_sub_dir_dt(), argv[x]);
		if(!tent) {
			fprintf(stderr, "tup: Unable to find tupid for '%s'\n", argv[x]);
			goto out_err;
		}
		tup_entry_list_add(tent, prune_list);
	}

	if(!list_empty(prune_list)) {
		struct tup_entry *tent;
		struct node *n;
		struct node *tmp;

		list_for_each_entry(tent, prune_list, list) {
			n = find_node(g, tent->tnode.tupid);
			if(n) {
				mark_nodes(n);
			} else {
				printf("Node '%s' does not need to be updated.\n", tent->name.s);
			}
		}

		list_for_each_entry_safe(n, tmp, &g->node_list, list) {
			if(!n->parsing && n != g->root)
				if(prune_node(g, n, num_pruned) < 0)
					goto out_err;
		}
	}
	tup_entry_release_list();
	return 0;

out_err:
	tup_entry_release_list();
	return -1;
}

#ifdef TUP_GRAPH_DEBUGGING
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
		n, n->tent->name.s, n->tnode.tupid, list_empty(&n->incoming), n->expanded, color);
	list_for_each_entry(e, &n->edges, list) {
		fprintf(f, "tup%p -> tup%p [dir=back];\n", e->dest, n);
	}
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
#endif
