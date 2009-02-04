#define _ATFILE_SOURCE
#include "updater.h"
#include "graph.h"
#include "fileio.h"
#include "debug.h"
#include "db.h"
#include "parser.h"
#include "server.h"
#include "file.h"
#include "fslurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/file.h>
#include <sys/wait.h>

static int process_create_nodes(void);
static int process_update_nodes(void);
static int build_graph(struct graph *g, int flags);
static int add_file_cb(void *arg, struct db_node *dbn);
static int find_deps(struct graph *g, struct node *n);
static int execute_create(struct graph *g);
static int execute_update(struct graph *g);
static void show_progress(int n, int tot);

static int update(struct node *n);
static int var_replace(struct node *n);
static int delete_file(struct node *n);

static int do_show_progress;
static int do_keep_going;

int updater(int argc, char **argv)
{
	int upd_lock;
	int x;

	upd_lock = open(TUP_UPDATE_LOCK, O_RDONLY);
	if(upd_lock < 0) {
		perror(TUP_UPDATE_LOCK);
		return -1;
	}
	if(flock(upd_lock, LOCK_EX|LOCK_NB) < 0) {
		if(errno == EWOULDBLOCK) {
			printf("Waiting for lock...\n");
			if(flock(upd_lock, LOCK_EX) == 0)
				goto lock_success;
		}
		perror("flock");
		return -1;
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
		return -1;
	if(process_update_nodes() < 0)
		return -1;

	flock(upd_lock, LOCK_UN);
	close(upd_lock);
	return 0;
}

static int process_create_nodes(void)
{
	struct graph g;

	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(build_graph(&g, TUP_FLAGS_CREATE) < 0)
		return -1;
	if(execute_create(&g) < 0)
		return -1;
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int process_update_nodes(void)
{
	struct graph g;

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(build_graph(&g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g, TUP_FLAGS_DELETE) < 0)
		return -1;
	if(execute_update(&g) < 0)
		return -1;
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int build_graph(struct graph *g, int flags)
{
	struct node *cur;

	g->cur = g->root;
	if(tup_db_select_node_by_flags(add_file_cb, g, flags) < 0)
		return -1;

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

static int add_file_cb(void *arg, struct db_node *dbn)
{
	struct graph *g = arg;
	struct node *n;

	if(find_node(g, dbn->tupid, &n) < 0)
		return -1;
	if(n != NULL)
		goto edge_create;
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
	if(tup_db_select_node_by_link(add_file_cb, g, n->tupid) < 0)
		return -1;
	return 0;
}

static int execute_create(struct graph *g)
{
	struct node *root;
	int num_processed = 0;
	int rc = -1;

	if(g->num_nodes)
		printf("Parsing Tupfiles\n");

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
			if(n->type == TUP_NODE_DIR) {
				if(parse(n, g) < 0)
					goto out_err;
			} else if(n->type == TUP_NODE_VAR) {
			} else if(n->type==TUP_NODE_FILE || n->type==TUP_NODE_CMD) {
			} else {
				fprintf(stderr, "Error: Unknown node %lli named '%s' in create graph.\n", n->tupid, n->name);
				goto out_err;
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
		/* TODO: Clear just the create flag? */
/*		if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
			goto out;*/
		if(n->type == TUP_NODE_DIR) {
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
out_err:
	tup_db_rollback();
	return -1;
}

static int execute_update(struct graph *g)
{
	struct node *root;
	int num_processed = 0;
	int rc = -1;

	if(g->num_nodes)
		printf("Executing Commands\n");

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
				printf("[35mDelete[%lli]: %s[0m\n",
				       n->tupid, n->name);
				if(delete_file(n) < 0)
					goto out;
			} else if((n->type == TUP_NODE_DIR ||
				   n->type == TUP_NODE_VAR) &&
				  n->flags == TUP_FLAGS_DELETE) {
				printf("[35mDelete[%lli]: %s[0m\n",
				       n->tupid, n->name);
				if(delete_name_file(n->tupid) < 0)
					goto out;
			} else if(n->type == TUP_NODE_CMD) {
				if(n->flags & TUP_FLAGS_DELETE) {
					printf("[35mDelete[%lli]: %s[0m\n",
					       n->tupid, n->name);
					if(delete_name_file(n->tupid) < 0)
						goto out;
				} else {
					if(update(n) < 0) {
						if(do_keep_going)
							goto keep_going;
						goto out;
					}
				}
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
		if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
			goto out;
keep_going:
		if(n->type == TUP_NODE_CMD) {
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
	int dfd = -1;
	int curfd = -1;
	int print_name = 1;
	const char *name = n->name;
	tupid_t tupid;

	/* Commands that begin with a ',' are special var/sed commands */
	if(name[0] == ',')
		return var_replace(n);

	tupid = tup_db_create_dup_node(n->dt, n->name, n->type, TUP_FLAGS_NONE);
	if(tupid < 0)
		return -1;

	if(name[0] == '@') {
		print_name = 0;
		name++;
	}

	curfd = open(".", O_RDONLY);
	if(curfd < 0)
			goto err_delete_node;

	dfd = tup_db_open_tupid(n->dt);
	if(dfd < 0)
		goto err_close_curfd;
	fchdir(dfd);

	if(print_name)
		printf("%s\n", name);

	start_server();
	pid = fork();
	if(pid < 0) {
		perror("fork");
		goto err_close_dfd;
	}
	if(pid == 0) {
		execl("/bin/sh", "/bin/sh", "-c", name, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	stop_server();

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			if(write_files(tupid) < 0)
				goto err_cmd_failed;
		} else {
			goto err_cmd_failed;
		}
	}
	fchdir(curfd);

	close(dfd);
	close(curfd);
	delete_name_file(n->tupid);
	return 0;

err_cmd_failed:
	/* Make sure we process this command again next time (since the command
	 * could be here because of an input file modification, which will now
	 * be cleared).
	 */
	tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_MODIFY);
	fprintf(stderr, " *** Command %lli failed.\n", n->tupid);

err_close_dfd:
	fchdir(curfd);
	close(dfd);
err_close_curfd:
	close(curfd);
err_delete_node:
	delete_name_file(tupid);
	return -1;
}

static int var_replace(struct node *n)
{
	int dfd;
	int curfd;
	int ifd;
	int ofd;
	struct buf b;
	char *input;
	char *rbracket;
	char *p, *e;
	int rc = -1;

	if(n->name[0] != ',') {
		fprintf(stderr, "Error: var_replace command must begin with ','\n");
		return -1;
	}
	input = n->name + 1;
	while(isspace(*input))
		input++;

	curfd = open(".", O_RDONLY);
	if(curfd < 0)
		return -1;

	dfd = tup_db_open_tupid(n->dt);
	if(dfd < 0)
		goto err_close_curfd;
	fchdir(dfd);

	printf("%s\n", input);
	rbracket = strchr(input, '>');
	if(rbracket == NULL) {
		fprintf(stderr, "Unable to find '>' in var/sed command '%s'\n",
			input);
		goto err_close_dfd;
	}
	/* Use -1 since the string is '%s > %s' and we need to set the space
	 * before the '>' to 0.
	 */
	if(rbracket == input) {
		fprintf(stderr, "Error: the '>' symbol can't be at the start of the var/sed command.\n");
		return -1;
	}
	rbracket[-1] = 0;

	ifd = open(input, O_RDONLY);
	if(ifd < 0) {
		perror(input);
		goto err_close_dfd;
	}
	if(fslurp(ifd, &b) < 0) {
		goto err_close_ifd;
	}
	ofd = creat(rbracket+2, 0666);
	if(ofd < 0) {
		perror(rbracket+2);
		goto err_free_buf;
	}

	p = b.s;
	e = b.s + b.len;
	do {
		char *at;
		char *rat;
		at = p;
		while(at < e && *at != '@') {
			at++;
		}
		if(write(ofd, p, at-p) != at-p) {
			perror("write");
			goto err_close_ofd;
		}
		if(at >= e)
			break;

		p = at;
		rat = p+1;
		while(rat < e && (isalnum(*rat) || *rat == '_')) {
			rat++;
		}
		if(rat < e && *rat == '@') {
			tupid_t varid;
			varid = tup_db_write_var(p+1, rat-(p+1), ofd);
			if(varid < 0)
				return -1;
			if(tup_db_create_link(varid, n->tupid) < 0)
				return -1;
			p = rat + 1;
		} else {
			if(write(ofd, p, rat-p) != rat-p) {
				perror("write");
				goto err_close_ofd;
			}
			p = rat;
		}
		
	} while(p < e);

	rc = 0;

err_close_ofd:
	close(ofd);
err_free_buf:
	free(b.s);
err_close_ifd:
	close(ifd);
err_close_dfd:
	fchdir(curfd);
	close(dfd);
err_close_curfd:
	close(curfd);
	return rc;
}

static int delete_file(struct node *n)
{
	int dirfd;
	int rc = 0;

	if(delete_name_file(n->tupid) < 0)
		return -1;
	dirfd = tup_db_open_tupid(n->dt);
	if(dirfd < 0) {
		if(dirfd == -ENOENT) {
			/* If the directory doesn't exist, the file can't
			 * either
			 */
			return 0;
		} else {
			return -1;
		}
	}

	if(unlinkat(dirfd, n->name, 0) < 0) {
		/* Don't care if the file is already gone. */
		if(errno != ENOENT) {
			perror(n->name);
			rc = -1;
			goto out;
		}
	}

out:
	close(dirfd);
	return rc;
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
