#define _ATFILE_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "tup/graph.h"
#include "tup/flist.h"
#include "tup/fileio.h"
#include "tup/tupid.h"
#include "tup/debug.h"
#include "tup/config.h"
#include "tup/compat.h"

static int process_create_nodes(void);
static int build_graph(struct graph *g);
static int process_tup_dir(const char *dir, struct graph *g, int type);
static int add_file(struct graph *g, const tupid_t tupid, struct node *src,
		    int type);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
int (*update)(const tupid_t tupid, char type);

int main(int argc, char **argv)
{
	struct graph g;
	struct tup_config cfg;
	int obj_lock;
	int upd_lock;
	void *handle;
	int x;

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("tup.updater");
		}
	}
	if(find_tup_dir() < 0) {
		return 1;
	}

	obj_lock = open(TUP_OBJECT_LOCK, O_RDONLY);
	if(obj_lock < 0) {
		perror(TUP_OBJECT_LOCK);
		return 1;
	}
	if(flock(obj_lock, LOCK_SH) < 0) {
		perror("flock");
		return 1;
	}

	upd_lock = open(TUP_UPDATE_LOCK, O_RDONLY);
	if(upd_lock < 0) {
		perror(TUP_UPDATE_LOCK);
		return 1;
	}
	if(flock(upd_lock, LOCK_EX|LOCK_NB) < 0) {
		if(errno == EWOULDBLOCK) {
			printf("Waiting for lock...\n");
			if(flock(upd_lock, LOCK_EX) == 0)
				goto lock_success;
		}
		perror("flock");
		return 1;
	}
lock_success:

	if(load_tup_config(&cfg) < 0) {
		return 1;
	}

	handle = dlopen(cfg.build_so, RTLD_LAZY);
	if(!handle) {
		fprintf(stderr, "Error: Unable to load %s\n", cfg.build_so);
		return 1;
	}
	update = dlsym(handle, "update");
	if(!update) {
		fprintf(stderr, "Error: Couldn't find 'update' symbol in "
			"builder.\n");
		return 1;
	}

	if(process_create_nodes() < 0)
		return 1;
	if(build_graph(&g) < 0)
		return 1;
	if(execute_graph(&g) < 0)
		return 1;

	flock(upd_lock, LOCK_UN);
	close(upd_lock);
	flock(obj_lock, LOCK_UN);
	close(obj_lock);
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
			if(update(f.filename, TUP_CREATE) < 0)
				return -1;
			if(move_tup_file("create", "modify", f.filename) < 0)
				return -1;
		}
		create_num++;
		if(create_num > 50) {
			fprintf(stderr, "Error: in create loop for 50 "
				"iterations - possible circular dependency?\n");
			return -1;
		}
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
	if(process_tup_dir(".tup/modify/", g, TUP_MODIFY) < 0)
		return -1;
	if(process_tup_dir(".tup/delete/", g, TUP_DELETE) < 0)
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
	char object_dir[] = ".tup/object/" SHA1_XD;
	char namefile[] = ".tup/object/" SHA1_XD "/.name";
	char depfile[] = ".tup/object/" SHA1_XD "/.secondary";
	struct stat st;
	struct stat st2;

	tupid_to_xd(object_dir + 12, n->tupid);
	flist_foreach(&f, object_dir) {
		if(f.filename[0] == '.')
			continue;
		if(strlen(f.filename) != sizeof(tupid_t)) {
			fprintf(stderr, "Error: invalid file '%s' in %s\n",
				f.filename, object_dir);
			return -1;
		}
		tupid_to_xd(namefile + 12, f.filename);
		tupid_to_xd(depfile + 12, f.filename);
		if(stat(namefile, &st) < 0)
			st.st_ino = -1;
		if(stat(depfile, &st2) < 0)
			st2.st_ino = -1;

		if(f._ent->d_ino != st.st_ino && f._ent->d_ino != st2.st_ino) {
			DEBUGP("Removing obsolete link %.*s -> %.*s\n",
			       8, n->tupid, 8, f.filename);
			unlinkat(f.dirfd, f.filename, 0);
			continue;
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
			int rc;
			if(n->type & TUP_DELETE) {
				if(num_dependencies(n->tupid) == 0) {
					delete_name_file(n->tupid);
					goto processed;
				}
			}
			rc = update(n->tupid, n->type);
			if(rc < 0)
				return -1;
		}
processed:
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
		if(n->type & TUP_MODIFY) {
			delete_tup_file("modify", n->tupid);
		}
		if(n->type & TUP_DELETE) {
			delete_tup_file("delete", n->tupid);
		}
		remove_node(n);
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		fprintf(stderr, "Error: Graph is not empty after execution.\n");
		return -1;
	}
	return 0;
}
