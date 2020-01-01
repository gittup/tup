/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2020  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "graph.h"
#include "entry.h"
#include "debug.h"
#include "fileio.h"
#include "config.h"
#include "db.h"
#include "container.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static struct graph group_graph;
static int group_graph_inited = 0;

static struct tup_entry root_entry;
static char root_name[] = "root";

struct node *find_node(struct graph *g, tupid_t tupid)
{
	struct tupid_tree *tnode;
	tnode = tupid_tree_search(&g->node_root, tupid);
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
	LIST_INIT(&n->edges);
	LIST_INIT(&n->incoming);
	n->tnode.tupid = tent->tnode.tupid;
	n->tent = tent;
	n->state = STATE_INITIALIZED;
	n->already_used = 0;
	n->expanded = 0;
	n->parsing = 0;
	n->marked = 0;
	n->skip = 1;
	TAILQ_INSERT_TAIL(&g->node_list, n, list);

	if(tupid_tree_insert(&g->node_root, &n->tnode) < 0)
		return NULL;
	return n;
}

static void remove_node_internal(struct graph *g, struct node *n)
{
	TAILQ_REMOVE(&g->node_list, n, list);
	while(!LIST_EMPTY(&n->edges)) {
		remove_edge(LIST_FIRST(&n->edges));
	}
	while(!LIST_EMPTY(&n->incoming)) {
		remove_edge(LIST_FIRST(&n->incoming));
	}
	remove_node(g, n);
}

void remove_node(struct graph *g, struct node *n)
{
	if(!LIST_EMPTY(&n->edges)) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tnode.tupid);
	}
	if(!LIST_EMPTY(&n->incoming)) {
		DEBUGP("Warning: Node %lli still has incoming edges.\n", n->tnode.tupid);
	}
	tupid_tree_rm(&g->node_root, &n->tnode);
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

	LIST_INSERT_HEAD(&n1->edges, e, list);
	LIST_INSERT_HEAD(&n2->incoming, e, destlist);

	return 0;
}

static int create_edge_sorted(struct node *n1, struct node *n2, int style)
{
	struct edge *e;
	struct edge *e2;
	struct edge *last;

	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;
	e->src = n1;
	e->style = style;

	last = NULL;
	LIST_FOREACH(e2, &n1->edges, list) {
		if(n2->tnode.tupid > e2->dest->tnode.tupid) {
			break;
		}
		last = e2;
	}
	if(last) {
		LIST_INSERT_AFTER(last, e, list);
	} else {
		LIST_INSERT_HEAD(&n1->edges, e, list);
	}

	last = NULL;
	LIST_FOREACH(e2, &n2->incoming, destlist) {
		if(n1->tnode.tupid > e2->src->tnode.tupid) {
			break;
		}
		last = e2;
	}
	if(last) {
		LIST_INSERT_AFTER(last, e, destlist);
	} else {
		LIST_INSERT_HEAD(&n2->incoming, e, destlist);
	}

	return 0;
}

void remove_edge(struct edge *e)
{
	LIST_REMOVE(e, list);
	LIST_REMOVE(e, destlist);
	free(e);
}

int create_graph(struct graph *g, enum TUP_NODE_TYPE count_flags, enum TUP_NODE_TYPE count_flags2)
{
	root_entry.tnode.tupid = 0;
	root_entry.dt = 0;
	root_entry.parent = NULL;
	root_entry.type = TUP_NODE_ROOT;
	root_entry.mtime = -1;
	root_entry.name.len = strlen(root_name);
	root_entry.name.s = root_name;
	RB_INIT(&root_entry.entries);

	TAILQ_INIT(&g->node_list);
	TAILQ_INIT(&g->plist);
	TAILQ_INIT(&g->removing_list);
	RB_INIT(&g->gen_delete_root);
	g->gen_delete_count = 0;
	RB_INIT(&g->cmd_delete_root);
	g->cmd_delete_count = 0;

	RB_INIT(&g->normal_dir_root);
	RB_INIT(&g->parse_gitignore_root);
	RB_INIT(&g->node_root);

	g->cur = g->root = create_node(g, &root_entry);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
	g->count_flags2 = count_flags2;
	g->total_mtime = 0;
	if(count_flags == TUP_NODE_GROUP)
		g->style = TUP_LINK_GROUP;
	else
		g->style = TUP_LINK_NORMAL;
	return 0;
}

int destroy_graph(struct graph *g)
{
	while(!TAILQ_EMPTY(&g->plist)) {
		remove_node_internal(g, TAILQ_FIRST(&g->plist));
	}
	while(!TAILQ_EMPTY(&g->node_list)) {
		remove_node_internal(g, TAILQ_FIRST(&g->node_list));
	}
	free_tupid_tree(&g->normal_dir_root);
	free_tupid_tree(&g->parse_gitignore_root);
	return 0;
}

void save_graphs(struct graph *g)
{
	save_graph(stderr, g, ".tup/tmp/graph-full-%i.dot");
	trim_graph(g);
	save_graph(stderr, g, ".tup/tmp/graph-trimmed-%i.dot");
}

static void expand_node(struct graph *g, struct node *n)
{
	n->expanded = 1;
	TAILQ_REMOVE(&g->node_list, n, list);
	TAILQ_INSERT_HEAD(&g->plist, n, list);
}

int build_graph_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;

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
		fprintf(stderr, "tup error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			g->cur->tnode.tupid, tent->tnode.tupid);
		save_graphs(g);
		return -1;
	}
	if(n->expanded == 0) {
		/* TUP_NODE_ROOT means we count everything */
		if(n->tent->type == g->count_flags || n->tent->type == g->count_flags2 || g->count_flags == TUP_NODE_ROOT) {
			g->num_nodes++;
			if(g->total_mtime != -1) {
				if(n->tent->mtime == -1)
					g->total_mtime = -1;
				else
					g->total_mtime += n->tent->mtime;
			}
		}
		expand_node(g, n);
	}

	if(g->cur == g->root) {
		n->skip = 0;
	}
	if(create_edge(g->cur, n, TUP_LINK_NORMAL) < 0)
		return -1;
	return 0;
}

static struct node *find_or_create_node(struct graph *g, struct tup_entry *tent)
{
	struct node *n;
	n = find_node(g, tent->tnode.tupid);
	if(n)
		return n;
	n = create_node(g, tent);
	if(!n) {
		fprintf(stderr, "tup error: Can't create node for tup_entry: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return NULL;
	}
	return n;
}

/* Callback for adding group dependencies to the regular update graph. This
 * just adds a link from <group1> -> <group2> so that if we 'tup upd
 * <group2>', we also build everything in group1. See t3088, t3089.
 */
int build_graph_group_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;

	n = find_or_create_node(g, tent);
	if(!n)
		return -1;

	if(n->expanded == 0) {
		expand_node(g, n);
	}

	if(create_edge(g->cur, n, TUP_LINK_NORMAL) < 0)
		return -1;

	return 0;
}

/* Callback for adding group dependencies to the circular dependency check
 * graph.
 */
static int build_group_cb(void *arg, struct tup_entry *tent,
			  struct tup_entry *cmdtent)
{
	struct graph *g = arg;
	struct node *cmdn;
	struct node *n;

	n = find_or_create_node(g, tent);
	if(!n)
		return -1;

	cmdn = find_or_create_node(g, cmdtent);
	if(!cmdn)
		return -1;

	if(n->expanded == 0) {
		expand_node(g, n);
	}
	if(cmdn->expanded == 0) {
		expand_node(g, cmdn);
	}

	if(create_edge(g->cur, cmdn, TUP_LINK_NORMAL) < 0)
		return -1;
	if(create_edge(cmdn, n, TUP_LINK_NORMAL) < 0)
		return -1;

	return 0;
}

int build_graph(struct graph *g)
{
	struct node *cur;

	while(!TAILQ_EMPTY(&g->plist)) {
		cur = TAILQ_FIRST(&g->plist);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", cur->tnode.tupid);
			g->cur = cur;
			if(g->style == TUP_LINK_GROUP) {
				if(tup_db_select_node_by_group_link(build_group_cb, g, cur->tnode.tupid) < 0)
					return -1;
			} else {
				if(tup_db_select_node_by_link(build_graph_cb, g, cur->tnode.tupid) < 0)
					return -1;
				if(g->cur->tent->type == TUP_NODE_GROUP) {
					if(tup_db_select_node_by_distinct_group_link(build_graph_group_cb, g, cur->tnode.tupid) < 0)
						return -1;
				}
			}
			cur->state = STATE_PROCESSING;
		} else if(cur->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %lli\n", cur->tnode.tupid);
			TAILQ_REMOVE(&g->plist, cur, list);
			TAILQ_INSERT_TAIL(&g->node_list, cur, list);
			cur->state = STATE_FINISHED;
		} else if(cur->state == STATE_FINISHED) {
			fprintf(stderr, "tup internal error: STATE_FINISHED node %lli in plist\n", cur->tnode.tupid);
			tup_db_print(stderr, cur->tnode.tupid);
			return -1;
		}
	}

	if(g->style != TUP_LINK_GROUP)
		if(add_graph_stickies(g) < 0)
			return -1;

	return 0;
}

int graph_empty(struct graph *g)
{
	if(g->node_list.tqh_last == &TAILQ_NEXT(g->root, list))
		return 1;
	return 0;
}

int add_graph_stickies(struct graph *g)
{
	struct node *n;

	TAILQ_FOREACH(n, &g->node_list, list) {
		if(n->tent->type == TUP_NODE_CMD) {
			struct tupid_entries sticky_root = {NULL};
			struct tupid_tree *tt;
			struct node *inputn;

			if(tup_db_get_inputs(n->tent->tnode.tupid, &sticky_root, NULL, NULL) < 0)
				return -1;
			RB_FOREACH(tt, tupid_entries, &sticky_root) {
				inputn = find_node(g, tt->tupid);
				if(inputn) {
					if(create_edge(inputn, n, TUP_LINK_STICKY) < 0)
						return -1;
				}
			}
			free_tupid_tree(&sticky_root);
		}
	}
	return 0;
}

static int add_file_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;

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
		fprintf(stderr, "tup error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			g->cur->tnode.tupid, tent->tnode.tupid);
		return -1;
	}
	if(n->expanded == 0) {
		if(n->tent->type == g->count_flags)
			g->num_nodes++;
		n->expanded = 1;
		TAILQ_REMOVE(&g->node_list, n, list);
		TAILQ_INSERT_HEAD(&g->plist, n, list);
	}

	if(create_edge(g->cur, n, TUP_LINK_NORMAL) < 0)
		return -1;
	return 0;
}

int nodes_are_connected(struct tup_entry *src, struct tupid_entries *valid_root,
			int *connected)
{
	struct graph g;
	struct node *n;

	if(create_graph(&g, TUP_NODE_CMD, -1) < 0)
		return -1;
	n = create_node(&g, src);
	if(!n)
		return -1;
	if(create_edge(g.cur, n, TUP_LINK_NORMAL) < 0)
		return -1;
	n->expanded = 1;
	TAILQ_REMOVE(&g.node_list, n, list);
	TAILQ_INSERT_HEAD(&g.plist, n, list);

	*connected = 0;
	while(!TAILQ_EMPTY(&g.plist)) {
		n = TAILQ_FIRST(&g.plist);

		if(src != n->tent &&
		   tupid_tree_search(valid_root, n->tent->tnode.tupid) != NULL) {
			*connected = 1;
			goto out_cleanup;
		}

		if(n->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", n->tnode.tupid);
			g.cur = n;
			if(tup_db_select_node_by_link(add_file_cb, &g, n->tnode.tupid) < 0)
				return -1;
			n->state = STATE_PROCESSING;
		} else if(n->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %lli\n", n->tnode.tupid);
			TAILQ_REMOVE(&g.plist, n, list);
			TAILQ_INSERT_TAIL(&g.node_list, n, list);
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
 */
static void mark_nodes(struct node *n)
{
	struct edge *e;

	/* If we're already marked, no need to go any further. */
	if(n->marked)
		return;

	n->marked = 1;

	/* A command node must have all of its outputs marked, or we risk not
	 * unlinking all of its outputs in the updater before running it
	 * (t6055).
	 */
	if(n->tent->type == TUP_NODE_CMD) {
		struct edge *e2;
		LIST_FOREACH(e2, &n->edges, list) {
			struct node *dest = e2->dest;

			/* Groups and exclusions are skipped, otherwise we end
			 * up building everything in the group (t3058, t5099).
			 */
			if(dest->tent->type != TUP_NODE_GROUP && dest->tent->dt != exclusion_dt()) {
				mark_nodes(dest);
			}
		}
	}

	/* Mark everything up the PDAG */
	LIST_FOREACH(e, &n->incoming, destlist) {
		struct node *mark = e->src;

		mark_nodes(mark);
	}
}

static int prune_node(struct graph *g, struct node *n, int *num_pruned, enum graph_prune_type gpt, int verbose)
{
	if(n->tent->type == g->count_flags && n->expanded) {
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

		g->num_nodes--;
		if(g->total_mtime != -1) {
			if(n->tent->mtime != -1)
				g->total_mtime -= n->tent->mtime;
		}
		(*num_pruned)++;
		if(verbose) {
			printf("Skipping: ");
			print_tup_entry(stdout, n->tent);
			printf("\n");
		}
	}
	/* Normal files are not pruned so we can make sure we update the mtime
	 * in the tup database (t6079). Any dependent commands are flagged as
	 * modify above, so they will still run later.
	 */
	if(gpt == GRAPH_PRUNE_ALL || n->tent->type != TUP_NODE_FILE) {
		remove_node_internal(g, n);
	}
	return 0;
}

int prune_graph(struct graph *g, int argc, char **argv, int *num_pruned,
		enum graph_prune_type gpt, int verbose)
{
	struct tup_entry_head *prune_list;
	struct tupid_entries dir_root = {NULL};
	int x;
	int dashdash = 0;
	int do_prune = 0;

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
		do_prune = 1;
		tent = get_tent_dt(get_sub_dir_dt(), argv[x]);
		if(!tent) {
			fprintf(stderr, "tup: Unable to find tupid for '%s'\n", argv[x]);
			goto out_err;
		}
		if(tent->type == TUP_NODE_DIR || tent->type == TUP_NODE_GENERATED_DIR) {
			/* For a directory, we recursively add all generated
			 * files in that directory, since updating the
			 * directory itself doesn't make sense for tup. This is
			 * done by putting all directories into dir_root, and
			 * then we can check all nodes to see if they are under
			 * one of these directories.
			 */
			if(tupid_tree_add_dup(&dir_root, tent->tnode.tupid) < 0)
				return -1;
		} else {
			tup_entry_list_add(tent, prune_list);
		}
	}

	if(do_prune) {
		struct tup_entry *tent;
		struct node *n;
		struct node *tmp;

		/* For explicit files: Just see if we have the node in the
		 * PDAG, and if so, mark it.
		 */
		LIST_FOREACH(tent, prune_list, list) {
			n = find_node(g, tent->tnode.tupid);
			if(n) {
				mark_nodes(n);
			}
		}

		/* For directories, we need to go through the list of nodes,
		 * and see if they have a parent in the dir tree.
		 */
		if(!RB_EMPTY(&dir_root)) {
			TAILQ_FOREACH(n, &g->node_list, list) {
				if(!n->marked && n->tent->type != TUP_NODE_ROOT) {
					struct tup_entry *dtent;
					dtent = n->tent->parent;
					while(dtent) {
						if(tupid_tree_search(&dir_root, dtent->tnode.tupid) != NULL) {
							mark_nodes(n);
						}
						dtent = dtent->parent;
					}
				}
			}
		}

		TAILQ_FOREACH_SAFE(n, &g->node_list, list, tmp) {
			if(!n->marked && n != g->root)
				if(prune_node(g, n, num_pruned, gpt, verbose) < 0)
					goto out_err;
		}
	}
	free_tupid_tree(&dir_root);
	tup_entry_release_list();
	return 0;

out_err:
	tup_entry_release_list();
	return -1;
}

void trim_graph(struct graph *g)
{
	struct node *n;
	struct node *tmp;
	int nodes_removed;
	do {
		nodes_removed = 0;
		TAILQ_FOREACH_SAFE(n, &g->node_list, list, tmp) {
			/* Get rid of any node that is either at the root or a
			 * leaf - nodes in the cycle will have both incoming
			 * and outgoing edges.
			 */
			if(LIST_EMPTY(&n->incoming) || LIST_EMPTY(&n->edges)) {
				remove_node_internal(g, n);
				nodes_removed = 1;
			}
		}
	} while(nodes_removed);
}

void save_graph(FILE *err, struct graph *g, const char *filename)
{
	static int count = 0;
	char realfile[PATH_MAX];
	FILE *f;

	if(snprintf(realfile, sizeof(realfile), filename, getpid(), count) < 0) {
		perror("asprintf");
		return;
	}
	if(err)
		fprintf(err, "tup: saving graph '%s'\n", realfile);

	count++;
	f = fopen(realfile, "w");
	if(!f) {
		perror(realfile);
		return;
	}
	dump_graph(g, f, 1, 0);
	fclose(f);
}

static void print_name(FILE *f, const char *s, int len)
{
	int x;
	for(x=0; x<len; x++) {
		if(s[x] == '"') {
			fprintf(f, "\\\"");
		} else if(s[x] == '\\') {
			fprintf(f, "\\\\");
		} else {
			fprintf(f, "%c", s[x]);
		}
	}
}

struct node_details {
	struct tupid_tree hashtnode;
	struct tupid_tree nodetnode;
	struct node *n;
	int count;
};

static void dump_node(FILE *f, struct graph *g, struct node *n,
		      int show_dirs, struct tupid_entries *node_root)
{
	int color;
	int fontcolor;
	const char *shape;
	const char *style;
	struct edge *e;
	int flags;
	struct tupid_tree *tt;
	tupid_t data;
	const char *datastring;

	if(n == g->root)
		return;

	style = "solid";
	color = 0;
	fontcolor = 0;
	switch(n->tent->type) {
		case TUP_NODE_FILE:
		case TUP_NODE_GENERATED:
			shape = "oval";
			break;
		case TUP_NODE_CMD:
			shape = "rectangle";
			break;
		case TUP_NODE_DIR:
			shape = "diamond";
			break;
		case TUP_NODE_GENERATED_DIR:
			shape = "house";
			break;
		case TUP_NODE_VAR:
			shape = "octagon";
			break;
		case TUP_NODE_GHOST:
			/* Ghost nodes won't have flags set */
			color = 0x888888;
			fontcolor = 0x888888;
			style = "dotted";
			shape = "oval";
			break;
		case TUP_NODE_GROUP:
			shape = "hexagon";
			break;
		case TUP_NODE_ROOT:
		default:
			shape="ellipse";
	}

	flags = tup_db_get_node_flags(n->tnode.tupid);
	if(flags & TUP_FLAGS_MODIFY) {
		color |= 0x0000ff;
		style = "dashed";
	}
	if(flags & TUP_FLAGS_CREATE) {
		color |= 0x00ff00;
		style = "dashed peripheries=2";
	}
	if(n->expanded == 0) {
		if(color == 0) {
			color = 0x888888;
			fontcolor = 0x888888;
		} else {
			/* Might only be graphing a subset. Ie:
			 * graph node foo, which points to command bar,
			 * and command bar is in the modify list. In
			 * this case, bar won't be expanded.
			 */
		}
	}
	fprintf(f, "\tnode_%lli [label=\"", n->tnode.tupid);
	if(n->tent->display)
		print_name(f, n->tent->display, n->tent->displaylen);
	else
		print_name(f, n->tent->name.s, n->tent->name.len);
	tt = tupid_tree_search(node_root, n->tent->tnode.tupid);
	if(tt) {
		struct node_details *nd;
		nd = container_of(tt, struct node_details, nodetnode);
		data = nd->count;
		if(n->tent->type == TUP_NODE_CMD)
			datastring = " commands";
		else
			datastring = " files";
	} else {
		data = n->tnode.tupid;
		datastring = "";
	}
	fprintf(f, "\\n");
	if(data != 1)
		fprintf(f, "%lli%s", data, datastring);
	fprintf(f, "\" shape=\"%s\" color=\"#%06x\" fontcolor=\"#%06x\" style=%s];\n", shape, color, fontcolor, style);


	if(show_dirs && n->tent->dt) {
		struct node *tmp;
		tmp = find_node(g, n->tent->dt);
		if(tmp)
			fprintf(f, "\tnode_%lli -> node_%lli [dir=back color=\"#888888\" arrowtail=odot];\n", n->tnode.tupid, n->tent->dt);
	}

	LIST_FOREACH(e, &n->edges, list) {
		fprintf(f, "\tnode_%lli -> node_%lli [dir=back,style=\"%s\",arrowtail=\"%s\"];\n", e->dest->tnode.tupid, n->tnode.tupid, (e->style == TUP_LINK_STICKY) ? "dotted" : "solid", (e->style & TUP_LINK_STICKY) ? "normal" : "empty");
	}
}

static tupid_t get_hash(tupid_t hash, tupid_t id, int style)
{
	return hash * 31 + (id * style);
}

static tupid_t command_name_hash(tupid_t hash, struct tup_entry *tent,
				 struct string_entries *root, int mult)
{
	const char *name;
	const char *space;
	int len;
	int x;

	/* Hash the command string up to the first
	 * space, so eg: all "gcc" commands can
	 * potentially be joined.
	 */
	name = tent->display;
	if(!name)
		name = tent->name.s;
	space = strchr(name, ' ');
	if(space) {
		len = space - name;
	} else {
		len = strlen(name);
	}

	if(root) {
		struct string_tree *st;
		if(string_tree_search(root, name, len) != NULL)
			return hash;
		st = malloc(sizeof *st);
		if(!st) {
			perror("malloc");
			return -1;
		}
		st->s = malloc(len + 1);
		strncpy(st->s, name, len);
		st->s[len] = 0;
		st->len = len;
		if(string_tree_insert(root, st) < 0)
			return -1;
	}

	for(x=0; x<len; x++) {
		hash = get_hash(hash, name[x], mult);
	}
	return hash;
}

static tupid_t command_outgoing_hash(tupid_t hash, struct node *n)
{
	struct edge *e;
	struct edge *e2;
	struct string_entries root = {NULL};

	LIST_FOREACH(e, &n->edges, list) {
		LIST_FOREACH(e2, &e->dest->edges, list) {
			if(e2->dest->tent->type == TUP_NODE_CMD) {
				hash = command_name_hash(hash, e2->dest->tent, &root, 1);
			}
		}
	}

	LIST_FOREACH(e, &n->edges, list) {
		LIST_FOREACH(e2, &e->dest->edges, list) {
			if(e2->dest->tent->type == TUP_NODE_CMD) {
				hash = command_outgoing_hash(hash, e2->dest);
			}
		}
	}
	free_string_tree(&root);
	return hash;
}

static tupid_t command_incoming_hash(tupid_t hash, struct node *n)
{
	struct edge *e;
	struct edge *e2;
	struct string_entries root = {NULL};

	LIST_FOREACH(e, &n->incoming, destlist) {
		LIST_FOREACH(e2, &e->src->incoming, destlist) {
			if(e2->src->tent->type == TUP_NODE_CMD) {
				hash = command_name_hash(hash, e2->src->tent, &root, -1);
			}
		}
	}

	LIST_FOREACH(e, &n->incoming, destlist) {
		LIST_FOREACH(e2, &e->src->incoming, destlist) {
			if(e2->src->tent->type == TUP_NODE_CMD) {
				hash = command_incoming_hash(hash, e2->src);
			}
		}
	}

	free_string_tree(&root);
	return hash;
}

static tupid_t command_hash_func(struct node *n)
{
	tupid_t hash = 1;

	/* Command hash is our name... */
	hash = command_name_hash(hash, n->tent, NULL, 1);

	/* Plus the hashes of all unique commands that follow... */
	hash = command_outgoing_hash(hash, n);

	/* Plus the hashes of all unique input commands */
	hash = command_incoming_hash(hash, n);
	return hash;
}

static tupid_t file_hash_func(struct node *n)
{
	struct edge *e;
	tupid_t hash = 1;
	LIST_FOREACH(e, &n->edges, list) {
		hash = get_hash(hash, e->dest->tnode.tupid, e->style);
	}
	LIST_FOREACH(e, &n->incoming, destlist) {
		/* incoming links add negative values
		 * to the hash, so A -> B has a
		 * different has from B -> C (ie: the
		 * input and output to a command should
		 * evaluate differently).
		 */
		hash = get_hash(hash, -e->src->tnode.tupid, e->style);
	}

	return hash;
}

static int find_edge(struct edge_head *eh, struct node *n, int style)
{
	struct edge *e;

	LIST_FOREACH(e, eh, list) {
		if(e->dest == n && e->style == style)
			return 1;
	}
	return 0;
}

static int find_incoming_edge(struct edge_head *eh, struct node *n, int style)
{
	struct edge *e;

	LIST_FOREACH(e, eh, destlist) {
		if(e->src == n && e->style == style)
			return 1;
	}
	return 0;
}

static int combine_nodes(struct graph *g, enum TUP_NODE_TYPE type, tupid_t (*hash_func)(struct node *n),
			 struct tupid_entries *hash_root, struct tupid_entries *node_root)
{
	struct node *n;
	struct node_details *nd;
	struct tupid_tree *tt;
	struct tupid_tree *tmp;
	struct tupid_entries tmproot = {NULL};

	/* First calculate all hashes */
	TAILQ_FOREACH(n, &g->node_list, list) {
		/* Funky since we do commands in one pass, then
		 * generated/normal files in another pass (which have multiple
		 * types)
		 */
		if((type == TUP_NODE_CMD && n->tent->type == TUP_NODE_CMD) ||
		   (type != TUP_NODE_CMD && n->tent->type != TUP_NODE_CMD)) {
			tupid_t hash;

			hash = hash_func(n);

			nd = malloc(sizeof *nd);
			if(!nd) {
				perror("malloc");
				return -1;
			}
			nd->hashtnode.tupid = hash;
			nd->nodetnode.tupid = n->tent->tnode.tupid;
			nd->count = 1;
			nd->n = n;
			tupid_tree_insert(&tmproot, &nd->nodetnode);
		}
	}

	/* Then keep only the first node of each hash value */
	RB_FOREACH_SAFE(tt, tupid_entries, &tmproot, tmp) {
		struct tupid_tree *hashtt;
		nd = container_of(tt, struct node_details, nodetnode);
		n = nd->n;

		hashtt = tupid_tree_search(hash_root, nd->hashtnode.tupid);
		if(hashtt == NULL) {
			tupid_tree_rm(&tmproot, &nd->nodetnode);
			tupid_tree_insert(hash_root, &nd->hashtnode);
			tupid_tree_insert(node_root, &nd->nodetnode);
		} else {
			struct node_details *count_nd;

			count_nd = container_of(hashtt, struct node_details, hashtnode);
			count_nd->count++;

			if(n->tent->type == TUP_NODE_CMD) {
				struct edge *e;
				/* When we combine a command, we move
				 * its inputs/outputs over to the
				 * command in the same hash bin so we
				 * can count them properly.
				 */
				LIST_FOREACH(e, &n->edges, list) {
					if(!find_edge(&count_nd->n->edges, e->dest, e->style)) {
						if(create_edge_sorted(count_nd->n, e->dest, e->style) < 0)
							return -1;
					}
				}
				LIST_FOREACH(e, &n->incoming, destlist) {
					if(!find_incoming_edge(&count_nd->n->incoming, e->src, e->style)) {
						if(create_edge_sorted(e->src, count_nd->n, e->style) < 0)
							return -1;
					}
				}
			}
			tupid_tree_rm(&tmproot, &nd->nodetnode);
			remove_node_internal(g, n);
			free(nd);
		}
	}
	return 0;
}

void dump_graph(struct graph *g, FILE *f, int show_dirs, int combine)
{
	struct node *n;
	struct tupid_entries hash_root = {NULL};
	struct tupid_entries node_root = {NULL};

	if(!show_dirs) {
		struct node *tmp;
		TAILQ_FOREACH_SAFE(n, &g->node_list, list, tmp) {
			int rmnode = 0;
			if(n->tent->type == TUP_NODE_DIR ||
			   n->tent->type == TUP_NODE_GENERATED_DIR)
				rmnode = 1;
			else if(n->tent->type == TUP_NODE_FILE &&
				(strcmp(n->tent->name.s, "Tupfile") == 0 ||
				 strcmp(n->tent->name.s, "Tupfile.lua") == 0 ||
				 strcmp(n->tent->name.s, "Tupdefault.lua") == 0
				 )
				)
				rmnode = 1;
			else if(n->tent->type == TUP_NODE_GENERATED &&
				strcmp(n->tent->name.s, ".gitignore") == 0)
				rmnode = 1;
			if(rmnode)
				remove_node_internal(g, n);
		}
	}

	if(combine) {
		if(combine_nodes(g, TUP_NODE_CMD, command_hash_func, &hash_root, &node_root) < 0)
			return;

		/* This also does generated nodes */
		if(combine_nodes(g, TUP_NODE_FILE, file_hash_func, &hash_root, &node_root) < 0)
			return;
	}

	fprintf(f, "digraph G {\n");
	TAILQ_FOREACH(n, &g->node_list, list) {
		if(RB_EMPTY(&node_root) || tupid_tree_search(&node_root, n->tent->tnode.tupid) != NULL) {
			dump_node(f, g, n, show_dirs, &node_root);
		}
	}
	TAILQ_FOREACH(n, &g->plist, list) {
		dump_node(f, g, n, show_dirs, &node_root);
	}

	while(!RB_EMPTY(&hash_root)) {
		struct tupid_tree *tt;
		struct node_details *nd;
		tt = RB_MIN(tupid_entries, &hash_root);
		nd = container_of(tt, struct node_details, hashtnode);
		tupid_tree_rm(&hash_root, &nd->hashtnode);
		tupid_tree_rm(&node_root, &nd->nodetnode);
		free(nd);
	}
	fprintf(f, "}\n");
}

int group_need_circ_check(void)
{
	return group_graph_inited;
}

int add_group_circ_check(struct tup_entry *tent)
{
	struct node *n;
	if(!group_graph_inited) {
		if(create_graph(&group_graph, TUP_NODE_GROUP, -1) < 0)
			return -1;
		group_graph_inited = 1;
	}
	if(find_node(&group_graph, tent->tnode.tupid) != NULL)
		return 0;

	n = create_node(&group_graph, tent);
	if(!n) {
		return -1;
	}
	TAILQ_REMOVE(&group_graph.node_list, n, list);
	TAILQ_INSERT_HEAD(&group_graph.plist, n, list);
	n->expanded = 1;
	return 0;
}

int group_circ_check(void)
{
	if(!group_graph_inited)
		return 0;
	if(build_graph(&group_graph) < 0)
		return -1;
	trim_graph(&group_graph);
	if(!TAILQ_EMPTY(&group_graph.node_list)) {
		struct node *n;
		fprintf(stderr, "tup error: Circular dependency found among the following groups:\n");
		TAILQ_FOREACH(n, &group_graph.node_list, list) {
			if(n->tent->type == TUP_NODE_GROUP) {
				fprintf(stderr, " - ");
				print_tup_entry(stderr, n->tent);
				fprintf(stderr, "\n");
			}
		}
		fprintf(stderr, "See the saved graph for the commands involved.\n");
		save_graph(stderr, &group_graph, ".tup/tmp/graph-group-circular-%i.dot");
		return -1;
	}
	return 0;
}
