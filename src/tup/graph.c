/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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
#include "mempool.h"
#include "entry.h"
#include "debug.h"
#include "fileio.h"
#include "config.h"
#include "db.h"
#include "container.h"
#include "compat.h"
#include "tent_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static struct graph group_graph;
static int group_graph_inited = 0;
static _Thread_local struct mempool node_pool = MEMPOOL_INITIALIZER(struct node);
static _Thread_local struct mempool edge_pool = MEMPOOL_INITIALIZER(struct edge);

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

	n = mempool_alloc(&node_pool);
	if(!n) {
		return NULL;
	}
	LIST_INIT(&n->edges);
	LIST_INIT(&n->incoming);
	n->tnode.tupid = tent->tnode.tupid;
	n->tent = tent;
	n->active_list = NULL;
	n->state = STATE_INITIALIZED;
	n->already_used = 0;
	n->expanded = 0;
	n->parsing = 0;
	n->marked = 0;
	n->skip = 1;
	n->counted = 0;
	if(node_insert_tail(&g->node_list, n) < 0)
		return NULL;

	/* The transient field in struct node is used for determining when to
	 * remove a transient file from the filesystem. It doesn't correspond
	 * to transient_list in the db.
	 */
	if(tent->type == TUP_NODE_GENERATED && is_transient_tent(tent)) {
		n->transient = TRANSIENT_PROCESSING;
	} else {
		n->transient = TRANSIENT_NONE;
	}

	if(tupid_tree_insert(&g->node_root, &n->tnode) < 0)
		return NULL;
	return n;
}

int node_insert_tail(struct node_head *head, struct node *n)
{
	if(n->active_list != NULL) {
		fprintf(stderr, "tup internal error: Node %s is already in a list.\n", n->tent->name.s);
		return -1;
	}
	TAILQ_INSERT_TAIL(head, n, list);
	n->active_list = head;
	return 0;
}

int node_insert_head(struct node_head *head, struct node *n)
{
	if(n->active_list != NULL) {
		fprintf(stderr, "tup internal error: Node %s is already in a list.\n", n->tent->name.s);
		return -1;
	}
	TAILQ_INSERT_HEAD(head, n, list);
	n->active_list = head;
	return 0;
}

int node_remove_list(struct node_head *head, struct node *n)
{
	if(!n->active_list) {
		fprintf(stderr, "tup internal error: Node %s is not on a list.\n", n->tent->name.s);
		return -1;
	}
	if(n->active_list != head) {
		fprintf(stderr, "tup internal error: Node %s being removed from a different list.\n", n->tent->name.s);
		return -1;
	}
	TAILQ_REMOVE(head, n, list);
	n->active_list = NULL;
	return 0;
}

static void remove_node_internal(struct graph *g, struct node *n)
{
	node_remove_list(n->active_list, n);
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
	mempool_free(&node_pool, n);
}

int create_edge(struct node *n1, struct node *n2, int style)
{
	struct edge *e;

	e = mempool_alloc(&edge_pool);
	if(!e) {
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

	e = mempool_alloc(&edge_pool);
	if(!e) {
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
	mempool_free(&edge_pool, e);
}

int create_graph(struct graph *g, enum TUP_NODE_TYPE count_flags)
{
	root_entry.tnode.tupid = 0;
	root_entry.dt = 0;
	root_entry.parent = NULL;
	root_entry.type = TUP_NODE_ROOT;
	root_entry.mtime = INVALID_MTIME;
	root_entry.name.len = strlen(root_name);
	root_entry.name.s = root_name;
	RB_INIT(&root_entry.entries);

	TAILQ_INIT(&g->node_list);
	TAILQ_INIT(&g->plist);
	TAILQ_INIT(&g->removing_list);
	tent_tree_init(&g->transient_root);
	tent_tree_init(&g->gen_delete_root);
	tent_tree_init(&g->save_root);
	tent_tree_init(&g->cmd_delete_root);

	tent_tree_init(&g->normal_dir_root);
	tent_tree_init(&g->parse_gitignore_root);
	RB_INIT(&g->node_root);

	g->cur = g->root = create_node(g, &root_entry);
	if(!g->root)
		return -1;
	g->num_nodes = 0;
	g->count_flags = count_flags;
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
	free_tent_tree(&g->normal_dir_root);
	free_tent_tree(&g->parse_gitignore_root);
	return 0;
}

void save_graphs(struct graph *g)
{
	save_graph(stderr, g, ".tup/tmp/graph-full-%i.dot");
	trim_graph(g);
	save_graph(stderr, g, ".tup/tmp/graph-trimmed-%i.dot");
}

static int expand_node(struct graph *g, struct node *n)
{
	n->expanded = 1;
	if(node_remove_list(&g->node_list, n) < 0)
		return -1;
	if(node_insert_head(&g->plist, n) < 0)
		return -1;
	return 0;
}

static int make_edge(struct graph *g, struct node *n)
{
	struct tup_entry *tent = n->tent;
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
		if(n->tent->type == g->count_flags || g->count_flags == TUP_NODE_ROOT) {
			n->counted = 1;
			g->num_nodes++;
			if(g->total_mtime != -1) {
				if(n->tent->mtime.tv_sec == -1)
					g->total_mtime = -1;
				else
					g->total_mtime += n->tent->mtime.tv_sec;
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

static int attach_transient_cb(void *arg, struct tup_entry *tent)
{
	/* Transient nodes that don't otherwise need to be built are only added
	 * if they can attach to something else in the DAG.
	 */
	struct graph *g = arg;
	struct node *n;
	n = find_node(g, tent->tnode.tupid);
	if(n) {
		return make_edge(g, n);
	}
	return 0;
}

int build_graph_transient_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;

	if(tent->type == TUP_NODE_CMD) {
		return build_graph_cb(g, tent);
	} else {
		struct node *n;
		n = find_node(g, tent->tnode.tupid);
		if(n == NULL) {
			n = create_node(g, tent);
			if(!n)
				return -1;
			if(make_edge(g, n) < 0)
				return -1;
			if(node_remove_list(&g->plist, n) < 0)
				return -1;
			if(node_insert_tail(&g->node_list, n) < 0)
				return -1;
			n->state = STATE_FINISHED;
		}
	}
	return 0;
}

int build_graph_non_transient_cb(void *arg, struct tup_entry *tent)
{
	/* Only build out nodes that aren't transient. Any transient nodes get
	 * saved in g->transient_root for later processing.
	 */
	struct graph *g = arg;
	if(tent->type == TUP_NODE_CMD && is_transient_tent(tent)) {
		if(tent_tree_add(&g->transient_root, tent) < 0)
			return -1;
		return 0;
	}

	return build_graph_cb(arg, tent);
}

int build_graph_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;

	n = find_node(g, tent->tnode.tupid);
	if(n == NULL) {
		n = create_node(g, tent);
		if(!n)
			return -1;
	}

	return make_edge(g, n);
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
 * just adds a link from <group1> -> <group2> so that if we 'tup
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
		if(expand_node(g, n) < 0)
			return -1;
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
		if(expand_node(g, n) < 0)
			return -1;
	}
	if(cmdn->expanded == 0) {
		if(expand_node(g, cmdn) < 0)
			return -1;
	}

	if(create_edge(g->cur, cmdn, TUP_LINK_NORMAL) < 0)
		return -1;
	if(create_edge(cmdn, n, TUP_LINK_NORMAL) < 0)
		return -1;

	return 0;
}

static int attach_transient_nodes(struct graph *g)
{
	struct tent_tree *tt;
	struct tent_tree *tmp;
	int rc = 0;
	RB_FOREACH_SAFE(tt, tent_entries, &g->transient_root, tmp) {
		struct tup_entry *tent;
		struct node *cmdnode;
		struct node *n;
		struct edge *e;
		int keep_cmd = 0;
		tent = tt->tent;

		if(find_node(g, tent->tnode.tupid)) {
			continue;
		}

		/* Expand the command and its output files */
		g->cur = create_node(g, tent);
		cmdnode = g->cur;
		if(tup_db_select_node_by_link(build_graph_cb, g, g->cur->tnode.tupid) < 0)
			return -1;
		LIST_FOREACH(e, &cmdnode->edges, list) {
			g->cur = e->dest;
			if(tup_db_select_node_by_sticky_link(attach_transient_cb, g, g->cur->tnode.tupid) < 0)
				return -1;

			/* We only need to keep the command if:
			 *  1) The file doesn't already exist from a previous
			 *     incomplete build (in the transient list)
			 *   and
			 *  2) We link to something in the DAG (!LIST_EMPTY)
			 */
			if(!tup_db_in_transient_list(g->cur->tent->tnode.tupid) && !LIST_EMPTY(&g->cur->edges)) {
				keep_cmd = 1;
				break;
			}
		}
		if(keep_cmd) {
			rc = 1;
			LIST_FOREACH(e, &cmdnode->edges, list) {
				n = e->dest;
				if(node_remove_list(&g->plist, n) < 0)
					return -1;
				if(node_insert_tail(&g->node_list, n) < 0)
					return -1;
				n->state = STATE_FINISHED;
			}
			g->cur = g->root;
			if(make_edge(g, cmdnode) < 0)
				return -1;
			if(node_remove_list(&g->plist, cmdnode) < 0)
				return -1;
			if(node_insert_tail(&g->node_list, cmdnode) < 0)
				return -1;
			tent_tree_rm(&g->transient_root, tt);
		} else {
			struct edge *tmpe;
			LIST_FOREACH_SAFE(e, &cmdnode->edges, list, tmpe) {
				if(!tup_db_in_transient_list(e->dest->tent->tnode.tupid)) {
					remove_node_internal(g, e->dest);
				}
			}
			remove_node_internal(g, cmdnode);
		}
	}
	return rc;
}

int build_graph(struct graph *g)
{
	struct node *cur;

	/* First fill out the graph with everything that we know needs to be
	 * built.
	 */
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
			if(node_remove_list(&g->plist, cur) < 0)
				return -1;
			if(node_insert_tail(&g->node_list, cur) < 0)
				return -1;
			cur->state = STATE_FINISHED;
		} else if(cur->state == STATE_FINISHED) {
			fprintf(stderr, "tup internal error: STATE_FINISHED node %lli in plist\n", cur->tnode.tupid);
			tup_db_print(stderr, cur->tnode.tupid);
			return -1;
		}
	}

	/* Second, attach any commands that build transient files that are
	 * currently missing. This has to be done recursively until no nodes
	 * are added in case there are chains of transient nodes.
	 */
	while(1) {
		int rc;
		rc = attach_transient_nodes(g);
		if(rc < 0)
			return -1;
		if(rc == 0)
			break;
	}
	/* Anything remaining in transient_root are nodes that we don't need to
	 * rebuild.
	 */
	free_tent_tree(&g->transient_root);

	/* Don't add graph stickies for groups, or for the create graph
	 * (count_flags == TUP_NODE_DIR). The latter is because we don't want
	 * to populate tup_entry->stickies before potentially deleting nodes
	 * (t6807).
	 */
	if(g->style != TUP_LINK_GROUP && g->count_flags != TUP_NODE_DIR)
		if(add_graph_stickies(g) < 0)
			return -1;

	if(!TAILQ_EMPTY(&g->plist)) {
		struct node *tmpn;
		fprintf(stderr, "tup internal error: plist should be empty after graph building.\n");
		TAILQ_FOREACH(tmpn, &g->plist, list) {
			printf(" - %s\n", tmpn->tent->name.s);
		}
		return -1;
	}
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
			struct tent_entries sticky_root = TENT_ENTRIES_INITIALIZER;
			struct tent_tree *tt;
			struct node *inputn;

			if(tup_db_get_inputs(n->tent->tnode.tupid, &sticky_root, NULL, NULL) < 0)
				return -1;
			RB_FOREACH(tt, tent_entries, &sticky_root) {
				inputn = find_node(g, tt->tent->tnode.tupid);
				if(inputn) {
					if(create_edge(inputn, n, TUP_LINK_STICKY) < 0)
						return -1;
				}
			}
			free_tent_tree(&sticky_root);
		}
	}
	return 0;
}

static int add_file_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;

	n = find_node(g, tent->tnode.tupid);
	if(n == NULL) {
		n = create_node(g, tent);
		if(!n)
			return -1;
	}

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
		n->expanded = 1;
		if(node_remove_list(&g->node_list, n) < 0)
			return -1;
		if(node_insert_head(&g->plist, n) < 0)
			return -1;
	}

	if(create_edge(g->cur, n, TUP_LINK_NORMAL) < 0)
		return -1;
	return 0;
}

int nodes_are_connected(struct tup_entry *src, struct tent_entries *valid_root,
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
	if(node_remove_list(&g.node_list, n) < 0)
		return -1;
	if(node_insert_head(&g.plist, n) < 0)
		return -1;

	*connected = 0;
	while(!TAILQ_EMPTY(&g.plist)) {
		n = TAILQ_FIRST(&g.plist);

		if(src != n->tent &&
		   tent_tree_search(valid_root, n->tent) != NULL) {
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
			if(node_remove_list(&g.plist, n) < 0)
				return -1;
			if(node_insert_tail(&g.node_list, n) < 0)
				return -1;
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
	struct edge *e;
	if(n->counted) {
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
			if(n->tent->mtime.tv_sec != -1)
				g->total_mtime -= n->tent->mtime.tv_sec;
		}
		(*num_pruned)++;
		if(verbose) {
			printf("Skipping: ");
			print_tup_entry(stdout, n->tent);
			printf("\n");
		}
	}
	/* If a partial update prunes a node from the DAG, we want to still
	 * keep any transient files we depend on around until they are actually
	 * used.
	 */
	LIST_FOREACH(e, &n->incoming, destlist) {
		if(e->src->transient)
			e->src->transient = TRANSIENT_NONE;
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
	struct tent_list_head prune_list;
	struct tupid_entries dir_root = {NULL};
	int x;
	int dashdash = 0;
	int do_prune = 0;

	*num_pruned = 0;

	tent_list_init(&prune_list);
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
			if(tent_list_add_head(&prune_list, tent) < 0)
				return -1;
		}
	}

	if(do_prune) {
		struct tent_list *tl;
		struct node *n;
		struct node *tmp;

		/* For explicit files: Just see if we have the node in the
		 * PDAG, and if so, mark it.
		 */
		tent_list_foreach(tl, &prune_list) {
			struct tup_entry *tent = tl->tent;
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
	free_tent_list(&prune_list);
	return 0;

out_err:
	free_tent_list(&prune_list);
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
		if(create_graph(&group_graph, TUP_NODE_GROUP) < 0)
			return -1;
		group_graph_inited = 1;
	}
	if(find_node(&group_graph, tent->tnode.tupid) != NULL)
		return 0;

	n = create_node(&group_graph, tent);
	if(!n) {
		return -1;
	}
	if(node_remove_list(&group_graph.node_list, n) < 0)
		return -1;
	if(node_insert_head(&group_graph.plist, n) < 0)
		return -1;
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
