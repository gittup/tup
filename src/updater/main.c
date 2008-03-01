#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "flist.h"
#include "graph.h"
#include "tupid.h"
#include "debug.h"

#define GRAPH_NAME "/home/marf/test%03i.dot"

static int build_graph(struct graph *g);
static int process_tup_dir(const char *dir, struct graph *g, int type);
static int add_file(struct graph *g, const tupid_t tupid, struct node *src,
		    int type);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static int update(struct node *n);
static void remove_if_exists(const char *path, const char *tupid);

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
	struct node *cur;

	if(create_graph(g, TUPDIR_HASH) < 0)
		return -1;

	/* First attach all nodes in the relevant .tup directories to the
	 * root.
	 */
	if(process_tup_dir(".tup/create/", g, TYPE_CREATE) < 0)
		return -1;
	if(process_tup_dir(".tup/modify/", g, TYPE_MODIFY) < 0)
		return -1;

	while(!list_empty(&g->plist)) {
		cur = list_entry(g->plist.next, struct node, list);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %.*s\n", 8, cur->tupid);
			if(find_deps(g, cur) < 0)
				return -1;
			cur->state = STATE_PROCESSING;
		} else if(cur->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %.*s\n", 8, cur->tupid);
			list_del(&cur->list);
			list_add_tail(&cur->list, &g->node_list);
			cur->state = STATE_FINISHED;
		}
	}

	dump_graph(g, GRAPH_NAME);
	return 0;
}

static int process_tup_dir(const char *dir, struct graph *g, int type)
{
	struct flist f;

	flist_foreach(&f, dir) {
		if(f.filename[0] == '.')
			continue;
		if(add_file(g, f.filename, g->root, type) < 0)
			return -1;
	}
	return 0;
}

static int add_file(struct graph *g, const tupid_t tupid, struct node *src,
		    int type)
{
	struct node *n;

	if((n = find_node(g, tupid)) != NULL) {
		if(!(n->type & type)) {
			DEBUGP("adding flag (0x%x) to %.*s\n", type, 8, tupid);
			n->type |= type;
		}
		goto edge_create;
	}
	n = create_node(tupid);
	if(!n)
		return -1;

	DEBUGP("create node: %.*s (0x%x)\n", 8, tupid, type);
	list_add(&n->list, &g->plist);
	n->state = STATE_INITIALIZED;
	n->type = type;

edge_create:
	if(n->state == STATE_PROCESSING) {
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
		if((rc = add_file(g, f.filename, n, n->type)) < 0)
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
			n->state = STATE_FINISHED;
			continue;
		}
		if(n != root) {
			if(update(n) < 0)
				return -1;
		}
		while(n->edges) {
			struct edge *e;
			e = n->edges;
			if(e->dest->state != STATE_PROCESSING) {
				list_del(&e->dest->list);
				list_add(&e->dest->list, &g->plist);
				e->dest->state = STATE_PROCESSING;
			}
			/* TODO: slist_del? */
			n->edges = remove_edge(e);
		}
		if(n->type & TYPE_CREATE) {
			remove_if_exists("create", n->tupid);
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
		char tstr[3];
		if(snprintf(tstr, sizeof(tstr), "%02x", n->type) >=
		   (signed)sizeof(tstr)) {
			fprintf(stderr, "Error: type didn't fit in string.\n");
			exit(1);
		}
		memcpy(tupid_str, n->tupid, sizeof(tupid_t));
		tupid_str[sizeof(tupid_str)-1] = 0;
		execl("/usr/bin/perl", "perl", "build", tstr, tupid_str, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0)
			return 0;
		fprintf(stderr, "Error: Update process failed with %i\n",
			WEXITSTATUS(status));
		return -WEXITSTATUS(status);
	}
	fprintf(stderr, "Error: Update process didn't return.\n");
	return -1;
}

static void remove_if_exists(const char *path, const char *tupid)
{
	struct stat buf;
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	memcpy(filename + 5, path, 6);
	memcpy(filename + 12, tupid, sizeof(tupid_t));
	if(stat(filename, &buf) < 0)
		return;
	if(S_ISREG(buf.st_mode))
		unlink(filename);
}
