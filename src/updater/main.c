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
#include "tup/slurp.h"
#include "tup/debug.h"
#include "tup/config.h"
#include "tup/compat.h"

static int process_create_nodes(void);
static int move_create_links(const tupid_t tupid);
static int build_graph(struct graph *g);
static int process_tup_dir(const char *dir, struct graph *g, int type);
static int add_file(struct graph *g, const tupid_t tupid, struct node *src,
		    int type);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static void show_progress(int n, int tot);

static int (*create)(const tupid_t tupid);
static int update(const tupid_t tupid);
static int delete_cmd(const tupid_t tupid);
static struct tup_config cfg;

int main(int argc, char **argv)
{
	struct graph g;
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
	create = dlsym(handle, "create");
	if(!create) {
		fprintf(stderr, "Error: Couldn't find 'create' symbol in "
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

			if(move_create_links(f.filename) < 0)
				return -1;
			if(create(f.filename) < 0)
				return -1;
			unlinkat(f.dirfd, f.filename, 0);
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

static int move_create_links(const tupid_t tupid)
{
	struct flist f;
	char objdir[] = ".tup/object/" SHA1_XD;
	char oldfile[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char newfile[] = ".tup/delete/" SHA1_X;

	tupid_to_xd(objdir+12, tupid);
	tupid_to_xd(oldfile+12, tupid);
	flist_foreach(&f, objdir) {
		if(f.filename[0] == '.')
			continue;
		memcpy(oldfile+14+sizeof(tupid_t), f.filename, sizeof(tupid_t));
		memcpy(newfile+12, f.filename, sizeof(tupid_t));
		unlink(newfile);
		if(rename(oldfile, newfile) < 0) {
			perror(newfile);
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
	struct stat st;
	char namefile[] = ".tup/object/" SHA1_XD "/.name";

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

	tupid_to_xd(namefile+12, tupid);
	if(stat(namefile, &st) == 0) {
		n->node = NODE_FILE;
	} else {
		n->node = NODE_CMD;
		g->num_nodes++;
	}
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
	char cmdfile[] = ".tup/object/" SHA1_XD "/.cmd";
	char depfile[] = ".tup/object/" SHA1_XD "/.secondary";
	struct stat st;
	struct stat st2;

	tupid_to_xd(object_dir + 12, n->tupid);
	flist_foreach(&f, object_dir) {
		int type;
		if(f.filename[0] == '.')
			continue;
		if(strlen(f.filename) != sizeof(tupid_t)) {
			fprintf(stderr, "Error: invalid file '%s' in %s\n",
				f.filename, object_dir);
			return -1;
		}
		if(n->node == NODE_FILE) {
			tupid_to_xd(cmdfile+12, f.filename);
			tupid_to_xd(depfile+12, f.filename);
			if(stat(cmdfile, &st) < 0)
				st.st_ino = -1;
			if(stat(depfile, &st2) < 0)
				st2.st_ino = -1;

			if(f._ent->d_ino != st.st_ino &&
			   f._ent->d_ino != st2.st_ino) {
				DEBUGP("Removing obsolete link %.*s -> %.*s\n",
				       8, n->tupid, 8, f.filename);
				unlinkat(f.dirfd, f.filename, 0);
				continue;
			}
		}

		/* Deleting a file doesn't mean the command should also be
		 * deleted. We just mark the command as modified so it will
		 * be re-executed.
		 */
		type = n->type;
		if(n->node == NODE_FILE)
			type = TUP_MODIFY;

		if((rc = add_file(g, f.filename, n, type)) < 0)
			break;
	};

	return rc;
}

static int execute_graph(struct graph *g)
{
	struct node *root;
	int num_processed = 0;

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %.*s\n", 8, root->tupid);
	list_move(&root->list, &g->plist);

	show_progress(num_processed, g->num_nodes);
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
			if(n->node == NODE_FILE && (n->type & TUP_DELETE)) {
				if(num_dependencies(n->tupid) == 0) {
					delete_name_file(n->tupid);
				}
			}
			if(n->node == NODE_CMD) {
				if(n->type & TUP_DELETE) {
					if(delete_cmd(n->tupid) < 0)
						return -1;
				} else {
					if(update(n->tupid) < 0)
						return -1;
				}
				num_processed++;
				show_progress(num_processed, g->num_nodes);
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

static int update(const tupid_t tupid)
{
	struct buf b;
	char cmdfile[] = ".tup/object/" SHA1_XD "/.cmd";

	tupid_to_xd(cmdfile+12, tupid);
	if(slurp(cmdfile, &b) < 0) {
		perror(cmdfile);
		return -1;
	}

	/* Overwrite newline */
	b.s[b.len-1] = 0;
	printf("%s\n", b.s);
	if(system(b.s) != 0)
		return -1;
	return 0;
}

static int delete_cmd(const tupid_t tupid)
{
	struct flist f;
	char cmdfile[] = ".tup/object/" SHA1_XD "/.cmd";
	char depfile[] = ".tup/object/" SHA1_XD "/.secondary";

	printf("[31mDelete %.*s[0m\n", 8, tupid);
	tupid_to_xd(cmdfile+12, tupid);
	tupid_to_xd(depfile+12, tupid);
	if(delete_if_exists(cmdfile) < 0)
		return -1;
	if(delete_if_exists(depfile) < 0)
		return -1;

	/* Change last / to nul to get dir name */
	cmdfile[13 + sizeof(tupid_t)] = 0;
	flist_foreach(&f, cmdfile) {
		if(f.filename[0] != '.') {
			if(create_tup_file_tupid("delete", f.filename) < 0)
				return -1;
			if(unlinkat(f.dirfd, f.filename, 0) < 0) {
				perror(f.filename);
				return -1;
			}
		}
	}
	if(rmdir(cmdfile) < 0) {
		perror(cmdfile);
		return -1;
	}
	return 0;
}

static void show_progress(int n, int tot)
{
	if(cfg.show_progress && tot) {
		int x, a, b;
		const int max = 40;
		char c = '=';
		if(tot > max) {
			a = n * max / tot;
			b = max;
			c = '#';
		} else {
			a = n;
			b = tot;
		}
		printf("[");
		for(x=0; x<a; x++) {
			printf("%c", c);
		}
		for(x=a; x<b; x++) {
			printf(" ");
		}
		printf("] %i/%i (%3i%%) ", n, tot, n*100/tot);
		if(n == tot)
			printf("\n");
	}
}
