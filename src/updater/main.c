#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "flist.h"
#include "graph.h"
#include "tupid.h"
#include "debug.h"

#define GRAPH_NAME "/home/marf/test%03i.dot"

static int build_graph(struct graph *g);
static int add_file(struct graph *g, const tupid_t tupid, struct node *src);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static int update(struct node *n);

int main(void)
{
	struct graph g;

	debug_enable("tup.updater");
	if(build_graph(&g) < 0)
		return 1;
	if(execute_graph(&g) < 0)
		return 1;
	return 0;
}

static int build_graph(struct graph *g)
{
	struct flist f;
	unsigned int x;
	struct node *root;
	struct node *cur;
	char add_pathnames[][13] = {
		".tup/create/",
		".tup/modify/",
	};

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);

	root = create_node(TUPDIR_HASH);
	if(!root)
		return -1;
	list_add(&root->list, &g->node_list);

	/* First attach all nodes in the relevant .tup directories to the
	 * root.
	 */
	for(x=0; x<sizeof(add_pathnames) / sizeof(add_pathnames[0]); x++) {
		flist_foreach(&f, add_pathnames[x]) {
			if(f.filename[0] == '.')
				continue;
			if(add_file(g, f.filename, root) < 0)
				return -1;
		}
	}

	while(!list_empty(&g->plist)) {
		cur = list_entry(g->plist.next, struct node, list);
		if(cur->state == NODE_INITIALIZED) {
			DEBUGP("find deps for node: %.*s\n", 8, cur->tupid);
			if(find_deps(g, cur) < 0)
				return -1;
			cur->state = NODE_PROCESSING;
		} else if(cur->state == NODE_PROCESSING) {
			DEBUGP("remove node from stack: %.*s\n", 8, cur->tupid);
			list_del(&cur->list);
			list_add_tail(&cur->list, &g->node_list);
			cur->state = NODE_FINISHED;
		}
	}

	dump_graph(g, GRAPH_NAME);
	return 0;
}

static int add_file(struct graph *g, const tupid_t tupid, struct node *src)
{
	struct node *n;

	if((n = find_node(g, tupid)) != NULL) {
		goto edge_create;
	}
	n = create_node(tupid);
	if(!n)
		return -1;

	DEBUGP("create node: %.*s\n", 8, tupid);
	list_add(&n->list, &g->plist);
	n->state = NODE_INITIALIZED;

edge_create:
	if(n->state == NODE_PROCESSING) {
		fprintf(stderr, "Error: Circular dependency detected! "
			"Last edge was: %.*s -> %.*s\n",
			8, src->tupid, 8, tupid);
		return -1;
	}
	if(create_edge(src, n) < 0)
		return -1;
	return 0;
}

static int find_deps(struct graph *g, struct node *n)
{
	int rc = 0;
	struct flist f;
	char object_dir[] = ".tup/object/" SHA1_X;

	memcpy(object_dir + 12, n->tupid, sizeof(tupid_t));
	flist_foreach(&f, object_dir) {
		if(f.filename[0] == '.')
			continue;
		if(strcmp(f.filename, "cmd") == 0 ||
		   strcmp(f.filename, "name") == 0)
			continue;
		if((rc = add_file(g, f.filename, n)) < 0)
			break;
	};

	return rc;
}

static int execute_graph(struct graph *g)
{
	struct node *root;

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %.*s\n", 8, root->tupid);
	list_move(&root->list, &g->plist);

	while(!list_empty(&g->plist)) {
		struct node *n;
		n = list_entry(g->plist.next, struct node, list);
		DEBUGP("cur node: %.*s [%i]\n", 8, n->tupid, n->incoming_count);
		if(n->incoming_count) {
			list_move(&n->list, &g->node_list);
			n->state = NODE_FINISHED;
			continue;
		}
		if(n != root) {
			update(n);
		}
		while(n->edges) {
			struct edge *e;
			e = n->edges;
			if(e->dest->state != NODE_PROCESSING) {
				list_del(&e->dest->list);
				list_add_tail(&e->dest->list, &g->plist);
				e->dest->state = NODE_PROCESSING;
			}
			/* TODO: slist_del? */
			n->edges = remove_edge(e);
		}
		remove_node(n);
		dump_graph(g, GRAPH_NAME);
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		fprintf(stderr, "Error: Graph is not empty after execution.\n");
		return -1;
	}
	return 0;
}

static int update(struct node *n)
{
	int pid;
	int status;

	pid = fork();
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(pid == 0) {
		char tupid_str[sizeof(tupid_t)+1];
		memcpy(tupid_str, n->tupid, sizeof(tupid_t));
		tupid_str[sizeof(tupid_str)-1] = 0;
		execl("/bin/sh", "sh", "build.sh", tupid_str, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0)
			return 0;
		fprintf(stderr, "Error: Update process failed with %i\n",
			WEXITSTATUS(status));
		return WEXITSTATUS(status);
	}
	fprintf(stderr, "Error: Update process didn't return.\n");
	return -1;
}
