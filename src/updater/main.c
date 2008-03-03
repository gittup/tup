#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "graph.h"
#include "tup/flist.h"
#include "tup/fileio.h"
#include "tup/tupid.h"
#include "tup/debug.h"

#define GRAPH_NAME "/home/marf/test%03i.dot"

static int process_create_nodes(void);
static int build_graph(struct graph *g);
static int process_tup_dir(const char *dir, struct graph *g, int type);
static int add_file(struct graph *g, const tupid_t tupid, struct node *src,
		    int type);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static int update(const tupid_t tupid, char type);

int main(void)
{
	struct graph g;

	debug_enable("tup.updater");
	if(process_create_nodes() < 0)
		return 1;
	if(build_graph(&g) < 0)
		return 1;
	if(execute_graph(&g) < 0)
		return 1;
	return 0;
}

static int process_create_nodes(void)
{
	struct flist f;
	int found = 1;
	int create_num = 0;

	while(found) {
		found = 0;
		DEBUGP("processing create nodes (%i)\n", create_num);
		flist_foreach(&f, ".tup/create/") {
			if(f.filename[0] == '.')
				continue;
			if(strlen(f.filename) != sizeof(tupid_t)) {
				fprintf(stderr, "Error: invalid file '%s' in "
					".tup/create/\n", f.filename);
				return -1;
			}
			found = 1;
			update(f.filename, TYPE_CREATE);
			if(move_tup_file("create", "modify", f.filename) < 0)
				return -1;
		}
		create_num++;
	}
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
	if(process_tup_dir(".tup/modify/", g, TYPE_MODIFY) < 0)
		return -1;
	if(process_tup_dir(".tup/delete/", g, TYPE_DELETE) < 0)
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
		if(strlen(f.filename) != sizeof(tupid_t)) {
			fprintf(stderr, "Error: invalid file '%s' in %s\n",
				f.filename, dir);
			return -1;
		}
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
		if(strlen(f.filename) != sizeof(tupid_t)) {
			fprintf(stderr, "Error: invalid file '%s' in %s\n",
				f.filename, object_dir);
			return -1;
		}
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
			if(n->type & TYPE_DELETE) {
				int rc;
#if 0
				int ndeps;
				ndeps = num_dependencies(n->tupid);
				if(ndeps < 0)
					return -1;
				if(ndeps == 0) {
					DEBUGP("delete orphaned node %.*s\n",
					       8, n->tupid);
					if(delete_name_file(n->tupid) < 0)
						return -1;
				} else {
					DEBUGP("deleted node %.*s still has %i "
					       "incoming edges: rebuild\n",
					       8, n->tupid, ndeps);
				}
				rc = update(n->tupid, TYPE_DELETE);
				/* TODO: better way than returning a
				 * special error code
				 */
				if(rc == -7 && delete_name_file(n->tupid) < 0)
					return -1;
				if(rc < 0)
					return -1;
#endif
				rc = update(n->tupid, TYPE_DELETE);
				/* TODO: better way than returning a
				 * special error code
				 */
				if(rc == -7 && delete_name_file(n->tupid) < 0)
					return -1;
				if(rc < 0 && rc != -7)
					return -1;
			} else if(n->type & TYPE_MODIFY) {
				if(update(n->tupid, TYPE_MODIFY) < 0)
					return -1;
			}
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
		if(n->type & TYPE_MODIFY) {
			delete_tup_file("modify", n->tupid);
		}
		if(n->type & TYPE_DELETE) {
			delete_tup_file("delete", n->tupid);
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

static int update(const tupid_t tupid, char type)
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
		if(snprintf(tstr, sizeof(tstr), "%02x", type) >=
		   (signed)sizeof(tstr)) {
			fprintf(stderr, "Error: type didn't fit in string.\n");
			exit(1);
		}
		memcpy(tupid_str, tupid, sizeof(tupid_t));
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
