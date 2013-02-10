/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2013  Mike Shal <marfey@gmail.com>
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

void remove_edge(struct edge *e)
{
	LIST_REMOVE(e, list);
	LIST_REMOVE(e, destlist);
	free(e);
}

int create_graph(struct graph *g, enum TUP_NODE_TYPE count_flags)
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

	RB_INIT(&g->node_root);

	g->cur = g->root = create_node(g, &root_entry);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
	g->total_mtime = 0;
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
	return 0;
}

int graph_empty(struct graph *g)
{
	if(g->node_list.tqh_last == &TAILQ_NEXT(g->root, list))
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
		fprintf(stderr, "tup error: Circular dependency detected! "
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
		TAILQ_REMOVE(&g->node_list, n, list);
		TAILQ_INSERT_HEAD(&g->plist, n, list);
	}

	if(create_edge(g->cur, n, style) < 0)
		return -1;
	return 0;
}

int nodes_are_connected(struct tup_entry *src, struct tupid_entries *valid_root,
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
	LIST_FOREACH(e, &n->incoming, destlist) {
		struct node *mark = e->src;

		mark_nodes(mark);

		/* A command node must have all of its outputs marked, or we
		 * risk not unlinking all of its outputs in the updater before
		 * running it (t6055).
		 */
		if(mark->tent->type == TUP_NODE_CMD) {
			struct edge *e2;
			LIST_FOREACH(e2, &mark->edges, list) {
				struct node *dest = e2->dest;

				/* Groups are skipped, otherwise we end up
				 * building everything in the group (t3058).
				 */
				if(dest->tent->type != TUP_NODE_GROUP) {
					mark_nodes(dest);
				}
			}
		}
	}
}

static int prune_node(struct graph *g, struct node *n, int *num_pruned)
{
	if(n->tent->type == g->count_flags && n->expanded) {
		g->num_nodes--;
		if(g->total_mtime != -1) {
			if(n->tent->mtime != -1)
				g->total_mtime -= n->tent->mtime;
		}
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
	struct tup_entry_head *prune_list;
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
		if(tent->type == TUP_NODE_DIR) {
			/* For a directory, we recursively add all generated
			 * files in that directory, since updating the
			 * directory itself doesn't make sense for tup.
			 */
			if(tup_db_get_generated_tup_entries(tent->tnode.tupid, prune_list) < 0)
				goto out_err;
		} else {
			tup_entry_list_add(tent, prune_list);
		}
	}

	if(do_prune) {
		struct tup_entry *tent;
		struct node *n;
		struct node *tmp;

		LIST_FOREACH(tent, prune_list, list) {
			n = find_node(g, tent->tnode.tupid);
			if(n) {
				mark_nodes(n);
			}
		}

		TAILQ_FOREACH_SAFE(n, &g->node_list, list, tmp) {
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

void save_graph(struct graph *g, const char *filename)
{
	static int count = 0;
	char realfile[PATH_MAX];
	FILE *f;

	if(snprintf(realfile, sizeof(realfile), filename, getpid(), count) < 0) {
		perror("asprintf");
		return;
	}
	fprintf(stderr, "tup: saving graph '%s'\n", realfile);

	count++;
	f = fopen(realfile, "w");
	if(!f) {
		perror(realfile);
		return;
	}
	dump_graph(g, f, 0, 0, 0);
	fclose(f);
}

static void print_name(FILE *f, const char *s, char c)
{
	for(; *s && *s != c; s++) {
		if(*s == '"') {
			fprintf(f, "\\\"");
		} else if(*s == '\\') {
			fprintf(f, "\\\\");
		} else {
			fprintf(f, "%c", *s);
		}
	}
}

static void dump_node(FILE *f, struct graph *g, struct node *n,
		      int show_dirs, int show_env, int show_ghosts)
{
	int color;
	int fontcolor;
	const char *shape;
	const char *style;
	char *s;
	struct edge *e;
	int flags;

	if(n == g->root)
		return;

	if(!show_env) {
		if(n->tent->tnode.tupid == env_dt() ||
		   n->tent->dt == env_dt())
			return;
	}

	style = "solid";
	color = 0;
	fontcolor = 0;
	switch(n->tent->type) {
		case TUP_NODE_FILE:
		case TUP_NODE_GENERATED:
			/* Skip Tupfiles in no-dirs mode since they
			 * point to directories.
			 */
			if(!show_dirs && strcmp(n->tent->name.s, "Tupfile") == 0)
				return;
			shape = "oval";
			break;
		case TUP_NODE_CMD:
			shape = "rectangle";
			break;
		case TUP_NODE_DIR:
			if(!show_dirs)
				return;
			shape = "diamond";
			break;
		case TUP_NODE_VAR:
			shape = "octagon";
			break;
		case TUP_NODE_GHOST:
			if(!show_ghosts)
				return;
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
	s = n->tent->name.s;
	if(s[0] == '^') {
		s++;
		while(*s && *s != ' ') {
			/* Skip flags (Currently there are none) */
			s++;
		}
		print_name(f, s, '^');
	} else {
		print_name(f, s, 0);
	}
	fprintf(f, "\\n%lli\" shape=\"%s\" color=\"#%06x\" fontcolor=\"#%06x\" style=%s];\n", n->tnode.tupid, shape, color, fontcolor, style);
	if(show_dirs && n->tent->dt) {
		struct node *tmp;
		tmp = find_node(g, n->tent->dt);
		if(tmp)
			fprintf(f, "\tnode_%lli -> node_%lli [dir=back color=\"#888888\" arrowtail=odot]\n", n->tnode.tupid, n->tent->dt);
	}

	LIST_FOREACH(e, &n->edges, list) {
		fprintf(f, "\tnode_%lli -> node_%lli [dir=back,style=\"%s\",arrowtail=\"%s\"]\n", e->dest->tnode.tupid, n->tnode.tupid, (e->style == TUP_LINK_STICKY) ? "dotted" : "solid", (e->style & TUP_LINK_STICKY) ? "normal" : "empty");
	}
}

void dump_graph(struct graph *g, FILE *f, int show_dirs, int show_env, int show_ghosts)
{
	struct node *n;

	fprintf(f, "digraph G {\n");
	TAILQ_FOREACH(n, &g->node_list, list) {
		dump_node(f, g, n, show_dirs, show_env, show_ghosts);
	}
	TAILQ_FOREACH(n, &g->plist, list) {
		dump_node(f, g, n, show_dirs, show_env, show_ghosts);
	}
	fprintf(f, "}\n");
}
