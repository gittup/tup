#include "updater.h"
#include "graph.h"
#include "fileio.h"
#include "debug.h"
#include "db.h"
#include "parser.h"
#include "server.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/wait.h>

static int create_flag_cb(void *arg, struct db_node *dbn);
static int process_create_nodes(void);
static int md_flag_cb(void *arg, struct db_node *dbn);
static int build_graph(struct graph *g);
static int add_file(struct graph *g, struct db_node *dbn);
static int find_deps(struct graph *g, struct node *n);
static int execute_graph(struct graph *g);
static void show_progress(int n, int tot);

static int update(struct node *n);
static int delete_file(struct node *n);

static int do_show_progress;
static int do_keep_going;

struct name_list {
	struct list_head list;
	char *name;
	tupid_t tupid;
};

int updater(int argc, char **argv)
{
	struct graph g;
	int upd_lock;
	int x;

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

	do_show_progress = tup_db_config_get_int("show_progress");
	do_keep_going = tup_db_config_get_int("keep_going");

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("tup.updater");
		} else if(strcmp(argv[x], "--show-progress") == 0) {
			do_show_progress = 1;
		} else if(strcmp(argv[x], "--no-show-progress") == 0) {
			do_show_progress = 0;
		} else if(strcmp(argv[x], "--keep-going") == 0 ||
			  strcmp(argv[x], "-k") == 0) {
			do_keep_going = 1;
		} else if(strcmp(argv[x], "--no-keep-going") == 0) {
			do_keep_going = 0;
		}
	}

	if(process_create_nodes() < 0)
		return 1;
	if(build_graph(&g) < 0)
		return 1;
	if(execute_graph(&g) < 0)
		return 1;

	flock(upd_lock, LOCK_UN);
	close(upd_lock);
	return 0;
}

static int create_flag_cb(void *arg, struct db_node *dbn)
{
	struct list_head *list = arg;
	struct name_list *nl;

	nl = malloc(sizeof *nl);
	if(!nl) {
		perror("malloc");
		return -1;
	}

	nl->name = strdup(dbn->name);
	if(!nl->name) {
		perror("strdup");
		return -1;
	}
	nl->tupid = dbn->tupid;

	list_add(&nl->list, list);

	/* TODO: Is this really valid to set here in the select callback?
	 * Maybe it should be moved into the while(!list_empty) loop in
	 * process_create_nodes()?
	 */
	/* Move all existing commands over to delete - then the ones that are
	 * re-created will be moved back out in create(). All those that are
	 * no longer generated remain in delete for cleanup.
	 */
	if(tup_db_set_dircmd_flags(dbn->tupid, TUP_FLAGS_DELETE) < 0)
		return -1;
	if(tup_db_set_cmd_output_flags(dbn->tupid, TUP_FLAGS_DELETE) < 0)
		return -1;

	return 0;
}

static int process_create_nodes(void)
{
	struct name_list *nl;
	LIST_HEAD(namelist);

	tup_db_begin();
	while(1) {
		if(tup_db_select_node_by_flags(create_flag_cb, &namelist,
					       TUP_FLAGS_CREATE) != 0)
			goto err_rollback;

		if(list_empty(&namelist))
			goto out_commit;

		while(!list_empty(&namelist)) {
			nl = list_entry(namelist.next, struct name_list, list);
			if(parser_create(nl->tupid) < 0)
				goto err_rollback;
			if(tup_db_set_flags_by_id(nl->tupid, TUP_FLAGS_NONE)<0)
				goto err_rollback;
			list_del(&nl->list);
			free(nl->name);
			free(nl);
		}
	}
out_commit:
	tup_db_commit();

	return 0;

err_rollback:
	tup_db_rollback();
	return -1;
}

static int md_flag_cb(void *arg, struct db_node *dbn)
{
	struct graph *g = arg;
	if(add_file(g, dbn) < 0)
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
	if(tup_db_select_node_by_flags(md_flag_cb, g, TUP_FLAGS_MODIFY) < 0)
		return -1;

	g->root->flags = TUP_FLAGS_DELETE;
	if(tup_db_select_node_by_flags(md_flag_cb, g, TUP_FLAGS_DELETE) < 0)
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

	return 0;
}

static int add_file(struct graph *g, struct db_node *dbn)
{
	struct node *n;

	if((n = find_node(g, dbn->tupid)) != NULL) {
		goto edge_create;
	}
	n = create_node(g, dbn);
	if(!n)
		return -1;
	DEBUGP("create node: %lli (0x%x)\n", dbn->tupid, dbn->type);

edge_create:
	if(n->state == STATE_PROCESSING) {
		fprintf(stderr, "Error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			g->cur->tupid, dbn->tupid);
		return -1;
	}
	if(create_edge(g->cur, n) < 0)
		return -1;
	return 0;
}

static int find_deps(struct graph *g, struct node *n)
{
	g->cur = n;
	if(tup_db_select_node_by_link(md_flag_cb, g, n->tupid) < 0)
		return -1;
	return 0;
}

static int execute_graph(struct graph *g)
{
	struct node *root;
	int num_processed = 0;
	int rc = -1;

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %lli\n", root->tupid);
	list_move(&root->list, &g->plist);

	tup_db_begin();
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
			   (n->flags == TUP_FLAGS_DELETE)) {
				delete_file(n);
			} else if(n->type == TUP_NODE_DIR &&
				  n->flags == TUP_FLAGS_DELETE) {
				printf("[35mDelete[%lli]: %s[0m\n",
				       n->tupid, n->name);
				if(delete_name_file(n->tupid) < 0)
					goto out;
			} else if(n->type == TUP_NODE_CMD) {
				if(n->flags & TUP_FLAGS_DELETE) {
					printf("[35mDelete[%lli]: %s[0m\n", n->tupid, n->name);
					if(delete_name_file(n->tupid) < 0)
						goto out;
				} else {
					if(update(n) < 0) {
						if(do_keep_going)
							goto keep_going;
						goto out;
					}
				}
			} else {
				printf("skip: %s\n", n->name);
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

			if(n->type == TUP_NODE_FILE) {
				if(n->type == TUP_FLAGS_DELETE ||
				   n->type == TUP_FLAGS_MODIFY)
					e->dest->flags |= TUP_FLAGS_MODIFY;
			} else if(n->type == TUP_NODE_CMD) {
				e->dest->flags |= n->flags;
			} else if(n->type == TUP_NODE_DIR) {
				e->dest->flags |= n->flags;
			}

			/* TODO: slist_del? */
			n->edges = remove_edge(e);
		}
		if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
			goto out;
keep_going:
		if(n != root) {
			num_processed++;
			show_progress(num_processed, g->num_nodes);
		}
		remove_node(g, n);
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		printf("\n");
		if(do_keep_going) {
			fprintf(stderr, "Remaining nodes skipped due to errors in command execution.\n");
		} else {
			fprintf(stderr, "Error: Graph is not empty after execution.\n");
		}
		goto out;
	}
	rc = 0;
out:
	tup_db_commit();
	return rc;
}

static int update(struct node *n)
{
	int status;
	int pid;
	int dfd;
	int curfd;
	tupid_t tupid;

	tupid = tup_db_create_dup_node(n->dt, n->name, n->type, TUP_FLAGS_NONE);
	if(tupid < 0)
		return -1;

	curfd = open(".", O_RDONLY);
	if(curfd < 0)
		goto err_delete_node;

	dfd = tup_db_opendir(n->dt);
	if(dfd < 0)
		goto err_close_curfd;
	fchdir(dfd);

	printf("%s\n", n->name);

	start_server();
	pid = fork();
	if(pid < 0) {
		perror("fork");
		goto err_close_dfd;
	}
	if(pid == 0) {
		execl("/bin/sh", "/bin/sh", "-c", n->name, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	stop_server();

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			if(write_files(tupid) < 0)
				goto err_close_dfd;
		} else {
			goto err_close_dfd;
		}
	}
	fchdir(curfd);

	close(dfd);
	close(curfd);
	delete_name_file(n->tupid);
	return 0;

err_close_dfd:
	fchdir(curfd);
	close(dfd);
err_close_curfd:
	close(curfd);
err_delete_node:
	delete_name_file(tupid);
	return -1;
}

static int delete_file(struct node *n)
{
	printf("[35mDelete[%lli]: %s[0m\n", n->tupid, n->name);
	if(delete_name_file(n->tupid) < 0)
		return -1;
	if(unlink(n->name) < 0) {
		/* Don't care if the file is already gone. */
		if(errno != ENOENT) {
			perror(n->name);
			return -1;
		}
	}

	return 0;
}

static void show_progress(int n, int tot)
{
	if(do_show_progress && tot) {
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
