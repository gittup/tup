#define _ATFILE_SOURCE
#include "updater.h"
#include "graph.h"
#include "flist.h"
#include "fileio.h"
#include "tupid.h"
#include "slurp.h"
#include "debug.h"
#include "config.h"
#include "compat.h"
#include "db.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>

static int create_flag_cb(void *arg, int argc, char **argv, char **col);
static int process_create_nodes(void);
static int md_flag_cb(void *arg, int argc, char **argv, char **col);
static int build_graph(struct graph *g);
static int add_file(struct graph *g, new_tupid_t tupid, struct node *src,
		    int type, const char *name);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static void show_progress(int n, int tot);

static int (*create)(const tupid_t tupid);
static int update(struct node *n);
static int delete_cmd(new_tupid_t tupid);
static struct tup_config cfg;
char make_so[] = "make.so";

struct name_list {
	struct list_head list;
	char *name;
	new_tupid_t tupid;
};

int updater(int argc, char **argv)
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

	/* TODO: load config */
	cfg.build_so = make_so;
	cfg.show_progress = 1;

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

static int create_flag_cb(void *arg, int argc, char **argv, char **col)
{
	int x;
	char *name;
	struct list_head *list = arg;
	struct name_list *nl;
	new_tupid_t id = -1;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "id") == 0)
			id = atoll(argv[x]);
		else if(strcmp(col[x], "name") == 0)
			name = argv[x];
	}

	if(id < 0) {
		fprintf(stderr, "Error: No valid ID for create node.\n");
		return -1;
	}

	/* Move all existing commands over to delete - then the ones that are
	 * re-created will be moved back out in create(). All those that are
	 * no longer generated remain in delete for cleanup.
	 */
	if(tup_db_exec("update node set flags=%i where id in (select to_id from link where from_id=%lli)", TUP_FLAGS_DELETE, id) != 0)
		return -1;

	nl = malloc(sizeof *nl);
	if(!nl) {
		perror("malloc");
		return -1;
	}

	nl->name = strdup(name);
	if(!nl->name) {
		perror("strdup");
		return -1;
	}
	nl->tupid = id;

	list_add(&nl->list, list);

	return 0;
}

static int process_create_nodes(void)
{
	struct name_list *nl;
	LIST_HEAD(namelist);

	/* TODO: Do in while loop in case it creates more create nodes? */
	if(tup_db_select(create_flag_cb, &namelist,
			 "select id, name from node where flags=%i",
			 TUP_FLAGS_CREATE) != 0)
		return -1;

	while(!list_empty(&namelist)) {
		nl = list_entry(namelist.next, struct name_list, list);
		if(create(nl->name) < 0)
			return -1;
		if(tup_db_exec("update node set flags=%i where id=%lli",
			       TUP_FLAGS_NONE, nl->tupid) != 0)
			return -1;
		list_del(&nl->list);
		free(nl->name);
		free(nl);
	}

	return 0;
}

static int md_flag_cb(void *arg, int argc, char **argv, char **col)
{
	struct graph *g = arg;
	char *name;
	int type;
	int flags;
	int x;
	new_tupid_t id;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "name") == 0)
			name = argv[x];
		else if(strcmp(col[x], "id") == 0)
			id = atoll(argv[x]);
		else if(strcmp(col[x], "type") == 0)
			type = atoll(argv[x]);
		else if(strcmp(col[x], "flags") == 0)
			flags = atoll(argv[x]);
	}
	if(add_file(g, id, g->cur, type, name) < 0)
		return -1;
	return 0;
}

static int build_graph(struct graph *g)
{
	struct node *cur;

	if(create_graph(g) < 0)
		return -1;

	g->cur = g->root;
	g->root->flags = TUP_FLAGS_MODIFY;
	if(tup_db_select(md_flag_cb, g, "select * from node where flags=%i", TUP_FLAGS_MODIFY) != 0)
		return -1;

	g->root->flags = TUP_FLAGS_DELETE;
	if(tup_db_select(md_flag_cb, g, "select * from node where flags=%i", TUP_FLAGS_DELETE) != 0)
		return -1;

	g->root->flags = TUP_FLAGS_NONE;

	while(!list_empty(&g->plist)) {
		cur = list_entry(g->plist.next, struct node, list);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", cur->tupid);
			if(find_deps(g, cur) < 0)
				return -1;
			cur->state = STATE_PROCESSING;
		} else if(cur->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %lli\n", cur->tupid);
			list_del(&cur->list);
			list_add_tail(&cur->list, &g->node_list);
			cur->state = STATE_FINISHED;
		}
	}

	dump_graph(g, "/home/marf/test1.dot");

	return 0;
}

static int add_file(struct graph *g, new_tupid_t tupid, struct node *src,
		    int type, const char *name)
{
	struct node *n;
	int flags;

	/* Inherit flags of the parent, unless the parent is a file, in which
	 * case we default to just modify (since a command is only deleted
	 * if the directory is modified and isn't re-created in the create
	 * phase. Yeah that totally makes sense.)
	 */
	flags = src->flags;
	if(src->type == TUP_NODE_FILE)
		flags = TUP_FLAGS_MODIFY;

	if((n = find_node(g, tupid)) != NULL) {
		if(!(n->flags & flags)) {
			DEBUGP("adding flag (0x%x) to %lli\n", flags, tupid);
			n->flags |= flags;
		}
		goto edge_create;
	}
	n = create_node(tupid, name);
	if(!n)
		return -1;

	n->type = type;
	if(n->type == TUP_NODE_CMD)
		g->num_nodes++;

	DEBUGP("create node: %lli (0x%x)\n", tupid, type);
	list_add(&n->list, &g->plist);
	n->state = STATE_INITIALIZED;
	n->flags = flags;

edge_create:
	if(n->state == STATE_PROCESSING) {
		fprintf(stderr, "Error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			src->tupid, tupid);
		return -1;
	}
	if(create_edge(src, n) < 0)
		return -1;
	return 0;
}

static int find_deps(struct graph *g, struct node *n)
{

	g->cur = n;
	if(tup_db_select(md_flag_cb, g,
			 "select * from node where id in (select to_id from link where from_id=%lli)", n->tupid) != 0)
		return -1;
	return 0;
}

static int execute_graph(struct graph *g)
{
	struct node *root;
	int num_processed = 0;

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %lli\n", root->tupid);
	list_move(&root->list, &g->plist);

	show_progress(num_processed, g->num_nodes);
	while(!list_empty(&g->plist)) {
		struct node *n;
		n = list_entry(g->plist.next, struct node, list);
		DEBUGP("cur node: %lli [%i]\n", n->tupid, n->incoming_count);
		if(n->incoming_count) {
			list_move(&n->list, &g->node_list);
			n->state = STATE_FINISHED;
			continue;
		}
		if(n != root) {
			if(n->type == TUP_NODE_FILE &&
			   (n->flags & TUP_FLAGS_DELETE)) {
#if 0
				TODO
				if(num_dependencies(n->tupid) == 0) {
					delete_name_file(n->tupid);
				}
#endif
			}
			if(n->type == TUP_NODE_CMD) {
				if(n->flags & TUP_FLAGS_DELETE) {
					if(delete_cmd(n->tupid) < 0)
						return -1;
				} else {
					if(update(n) < 0)
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
		if(tup_db_exec("update node set flags=%i where id=%lli",
			       TUP_FLAGS_NONE, n->tupid) != 0)
			return -1;
		remove_node(n);
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		fprintf(stderr, "Error: Graph is not empty after execution.\n");
		return -1;
	}
	return 0;
}

static int update(struct node *n)
{
	int rc;
	char s[32];

	if(snprintf(s, sizeof(s), "%lli", n->tupid) >= (signed)sizeof(s)) {
		fprintf(stderr, "Buffer size error in update()\n");
		return -1;
	}

	if(setenv(TUP_CMD_ID, s, 1) < 0) {
		perror("setenv");
		return -1;
	}
	printf("%s\n", n->name);
	rc = system(n->name);
	unsetenv(TUP_CMD_ID);
	if(rc != 0)
		return -1;
	return 0;
}

static int delete_cmd(new_tupid_t tupid)
{
	if(tup_db_exec("delete from node where id=%lli", tupid) != 0)
		return -1;

	/* TODO: Do we want to delete all links? I assume so */
	if(tup_db_exec("delete from link where from_id=%lli or to_id=%lli", tupid, tupid) != 0)
		return -1;

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
