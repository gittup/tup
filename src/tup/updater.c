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
#include "array_size.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>

static int process_create_nodes(void);
static int process_delete_nodes(void);
static int process_update_nodes(void);
static int build_graph(struct graph *g);
static int add_file_cb(void *arg, struct db_node *dbn, int style);
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *));

static void *create_work(void *arg);
static void *delete_work(void *arg);
static void *update_work(void *arg);
static int update(struct node *n, struct server *s);
static int var_replace(struct node *n);
static int delete_file(struct node *n);
static void sighandler(int sig);
static void show_progress(int sum, int tot, struct node *n);

static int do_show_progress;
static int do_keep_going;
static int num_jobs;

static int sig_quit = 0;
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESETHAND | SA_RESTART,
};

static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *signal_err[] = {
	NULL, /* 0 */
	"Hangup detected on controlling terminal or death of controlling process",
	"Interrupt from keyboard",
	"Quit from keyboard",
	"Illegal Instruction",
	NULL, /* 5 */
	"Abort signal from abort(3)",
	NULL,
	"Floating point exception",
	"Kill signal",
	NULL, /* 10 */
	"Segmentation fault",
	NULL,
	"Broken pipe: write to pipe with no readers",
	"Timer signal from alarm(2)",
	"Termination signal", /* 15 */
};

struct worker_thread {
	pthread_t pid;
	int sock;        /* 1 sock and no shoes? What a life... */
	struct graph *g; /* This should only be used in create_work() */
};

struct work_rc {
	struct node *n;
	int rc;
};

int updater(int argc, char **argv, int parse_only)
{
	int x;

	do_show_progress = tup_db_config_get_int("show_progress");
	do_keep_going = tup_db_config_get_int("keep_going");
	num_jobs = tup_db_config_get_int("num_jobs");

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
		} else if(strncmp(argv[x], "-j", 2) == 0) {
			num_jobs = strtol(argv[x]+2, NULL, 0);
		}
	}

	if(num_jobs < 1) {
		fprintf(stderr, "Warning: Setting the number of jobs to 1\n");
		num_jobs = 1;
	}

	if(server_init() < 0)
		return -1;
	if(process_create_nodes() < 0)
		return -1;
	if(parse_only)
		return 0;
	if(process_delete_nodes() < 0)
		return -1;
	if(process_update_nodes() < 0)
		return -1;
	return 0;
}

static int process_create_nodes(void)
{
	struct graph g;
	int rc;

	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes)
		printf("Parsing Tupfiles\n");
	tup_db_begin();
	rc = execute_graph(&g, 0, 1, create_work);
	if(rc == 0) {
		tup_db_commit();
	} else if(rc == -1) {
		tup_db_rollback();
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int process_delete_nodes(void)
{
	struct graph g;
	int rc;

	if(create_graph(&g, TUP_NODE_GENERATED) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_DELETE) < 0)
		return -1;
	if(tup_db_modify_deleted_deps() < 0)
		return -1;
	if(g.num_nodes)
		printf("Deleting %i file%s\n", g.num_nodes,
		       g.num_nodes == 1 ? "" : "s");
	tup_db_begin();
	rc = execute_graph(&g, 0, 1, delete_work);
	if(rc == 0) {
		tup_db_commit();
	} else if(rc == -1) {
		tup_db_rollback();
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int process_update_nodes(void)
{
	struct graph g;
	int rc;

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes)
		printf("Executing Commands\n");
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	tup_db_begin();
	rc = execute_graph(&g, do_keep_going, num_jobs, update_work);
	if(rc == -2) {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	tup_db_commit();
	if(rc < 0)
		return -1;
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static int build_graph(struct graph *g)
{
	struct node *cur;

	while(!list_empty(&g->plist)) {
		cur = list_entry(g->plist.next, struct node, list);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", cur->tupid);
			g->cur = cur;
			if(tup_db_select_node_by_link(add_file_cb, g, cur->tupid) < 0)
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

static int add_file_cb(void *arg, struct db_node *dbn, int style)
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
	if(style & TUP_LINK_NORMAL && n->expanded == 0) {
		if(n->type == g->count_flags)
			g->num_nodes++;
		n->expanded = 1;
		list_move(&n->list, &g->plist);
	}
	if(create_edge(g->cur, n, style) < 0)
		return -1;
	return 0;
}

static void pop_node(struct graph *g, struct node *n)
{
	while(n->edges) {
		struct edge *e;
		e = n->edges;
		if(e->dest->state != STATE_PROCESSING) {
			/* Put the node back on the plist, and mark it as such
			 * by changing the state to STATE_PROCESSING.
			 */
			list_move(&e->dest->list, &g->plist);
			e->dest->state = STATE_PROCESSING;
		}

		n->edges = remove_edge(e);
	}
}

/* Returns:
 *   0: everything built ok
 *  -1: a command failed
 *  -2: a system call failed (some work threads may still be active)
 */
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *))
{
	struct node *root;
	struct worker_thread *workers;
	int num_processed = 0;
	int socks[2];
	int rc = -1;
	int x;
	int active = 0;

	if(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socks) < 0) {
		perror("socketpair");
		return -2;
	}

	workers = malloc(sizeof(*workers) * jobs);
	if(!workers) {
		perror("malloc");
		return -2;
	}
	for(x=0; x<jobs; x++) {
		workers[x].g = g;
		workers[x].sock = socks[1];
		if(pthread_create(&workers[x].pid, NULL, work_func, &workers[x]) < 0) {
			perror("pthread_create");
			return -2;
		}
	}

	root = list_entry(g->node_list.next, struct node, list);
	DEBUGP("root node: %lli\n", root->tupid);
	list_del(&root->list);
	pop_node(g, root);
	remove_node(g, root);

	while(!list_empty(&g->plist) && !sig_quit) {
		struct node *n;
		n = list_entry(g->plist.next, struct node, list);
		DEBUGP("cur node: %lli [%i]\n", n->tupid, n->incoming_count);
		if(n->incoming_count) {
			/* Here STATE_FINISHED means we're on the node_list,
			 * therefore not ready for processing.
			 */
			list_move(&n->list, &g->node_list);
			n->state = STATE_FINISHED;
			goto check_empties;
		}

		if(!n->expanded) {
			list_del(&n->list);
			pop_node(g, n);
			remove_node(g, n);
			continue;
		}

		if(n->type == g->count_flags) {
			show_progress(num_processed, g->num_nodes, n);
			num_processed++;
		}
		list_del(&n->list);
		active++;
		if(send(socks[0], &n, sizeof(n), 0) != sizeof(n)) {
			perror("send");
			return -2;
		}

check_empties:
		/* Keep looking for dudes to return as long as:
		 *  1) There are no more free workers
		 *  2) There is no work to do (plist is empty or sigquit is
		 *     set) and some people are active.
		 */
		while(active == jobs ||
		      ((list_empty(&g->plist) || sig_quit) && active)) {
			int ret;
			struct work_rc wrc;

			/* recv() might get EINTR if we ctrl-c or kill tup */
			do {
				ret = recv(socks[0], &wrc, sizeof(wrc), 0);
				if(ret == sizeof(wrc))
					break;
				if(ret < 0 && errno != EINTR) {
					perror("recv");
					return -2;
				}
			} while(1);

			active--;
			if(wrc.rc < 0) {
				if(keep_going)
					goto keep_going;
				goto out;
			}
			pop_node(g, wrc.n);

keep_going:
			remove_node(g, wrc.n);
		}
	}
	if(!list_empty(&g->node_list) || !list_empty(&g->plist)) {
		printf("\n");
		if(do_keep_going) {
			fprintf(stderr, "Remaining nodes skipped due to errors in command execution.\n");
		} else {
			if(sig_quit) {
				fprintf(stderr, "Remaining nodes skipped due to caught signal.\n");
			} else {
				fprintf(stderr, "Error: Graph is not empty after execution.\n");
			}
		}
		goto out;
	}
	show_progress(num_processed, g->num_nodes, NULL);
	rc = 0;
out:
	for(x=0; x<jobs; x++) {
		struct node *n = NULL;
		send(socks[0], &n, sizeof(n), 0);
	}
	for(x=0; x<jobs; x++) {
		pthread_join(workers[x].pid, NULL);
	}
	free(workers); /* Viva la revolucion! */
	close(socks[0]);
	close(socks[1]);
	return rc;
}

static void *create_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;

	while(recv(wt->sock, &n, sizeof(n), 0) == sizeof(n)) {
		struct work_rc wrc;
		int rc = 0;

		if(n == NULL)
			break;

		if(n->type == TUP_NODE_DIR) {
			if(n->already_used) {
				printf("Already parsed[%lli]: '%s'\n", n->tupid, n->name);
				rc = 0;
			} else {
				rc = parse(n, g);
			}
		} else if(n->type == TUP_NODE_VAR ||
			  n->type == TUP_NODE_FILE ||
			  n->type == TUP_NODE_GENERATED ||
			  n->type == TUP_NODE_CMD) {
			rc = 0;
		} else {
			fprintf(stderr, "Error: Unknown node %lli named '%s' in create graph.\n", n->tupid, n->name);
			rc = -1;
		}
		if(tup_db_unflag_create(n->tupid) < 0)
			rc = -1;

		wrc.rc = rc;
		wrc.n = n;
		if(send(wt->sock, &wrc, sizeof(wrc), 0) != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	return NULL;
}

static void *delete_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct node *n;

	while(recv(wt->sock, &n, sizeof(n), 0) == sizeof(n)) {
		int rc = 0;
		struct work_rc wrc;

		if(n == NULL)
			break;

		if(n->flags & TUP_FLAGS_DELETE) {
			pthread_mutex_lock(&db_mutex);
			if(n->type == TUP_NODE_GENERATED) {
				rc = delete_file(n);
			} else {
				rc = delete_name_file(n->tupid);
			}

			if(rc == 0) {
				if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
					rc = -1;
			}
			pthread_mutex_unlock(&db_mutex);
		}

		wrc.rc = rc;
		wrc.n = n;
		if(send(wt->sock, &wrc, sizeof(wrc), 0) != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	return NULL;
}

static void *update_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct server *s;
	struct node *n;

	s = malloc(sizeof *s);
	if(!s) {
		perror("malloc");
		return NULL;
	}
	s->db_mutex = &db_mutex;

	while(recv(wt->sock, &n, sizeof(n), 0) == sizeof(n)) {
		struct edge *e;
		int rc = 0;
		struct work_rc wrc;

		if(n == NULL)
			break;

		if(n->type == TUP_NODE_CMD) {
			rc = update(n, s);
		}

		if(rc == 0) {
			e = n->edges;
			pthread_mutex_lock(&db_mutex);
			while(e) {
				/* Mark the next nodes as modify in case we hit
				 * an error - we'll need to pick up there.
				 */
				if(e->style & TUP_LINK_NORMAL) {
					if(tup_db_add_modify_list(e->dest->tupid) < 0)
						rc = -1;
				}
				e = e->next;
			}
			if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
				rc = -1;
			pthread_mutex_unlock(&db_mutex);
		}

		wrc.rc = rc;
		wrc.n = n;
		if(send(wt->sock, &wrc, sizeof(wrc), 0) != sizeof(wrc)) {
			perror("write");
			return NULL;
		}
	}
	free(s);
	return NULL;
}

static int update(struct node *n, struct server *s)
{
	int status;
	int pid;
	int dfd = -1;
	const char *name = n->name;
	int rc;
	tupid_t tupid;

	/* Commands that begin with a ',' are special var/sed commands */
	if(name[0] == ',') {
		pthread_mutex_lock(&db_mutex);
		rc = var_replace(n);
		pthread_mutex_unlock(&db_mutex);
		return rc;
	}

	if(name[0] == '^') {
		name++;
		while(*name && *name != ' ') {
			/* This space reserved for flags for something. I dunno
			 * what yet.
			 */
			fprintf(stderr, "Error: Unknown ^ flag: '%c'\n", *name);
			name++;
			return -1;
		}
		while(*name && *name != '^') name++;
		if(!*name) {
			fprintf(stderr, "Error: Missing ending '^' flag in command %lli: %s\n", n->tupid, n->name);
			return -1;
		}
		name++;
		while(isspace(*name)) name++;
	}

	pthread_mutex_lock(&db_mutex);
	tupid = tup_db_create_dup_node(n->dt, n->name, n->type);
	if(tupid < 0) {
		pthread_mutex_unlock(&db_mutex);
		return -1;
	}

	dfd = tup_db_open_tupid(n->dt);
	if(dfd < 0) {
		pthread_mutex_unlock(&db_mutex);
		goto err_delete_node;
	}

	pthread_mutex_unlock(&db_mutex);

	if(start_server(s) < 0) {
		fprintf(stderr, "Error starting update server.\n");
		goto err_close_dfd;
	}
	pid = fork();
	if(pid < 0) {
		perror("fork");
		goto err_close_dfd;
	}
	if(pid == 0) {
		struct sigaction sa = {
			.sa_handler = SIG_IGN,
			.sa_flags = SA_RESETHAND | SA_RESTART,
		};
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		fchdir(dfd);
		server_setenv(s);
		execl("/bin/sh", "/bin/sh", "-c", name, NULL);
		perror("execl");
		exit(1);
	}
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		goto err_cmd_failed;
	}
	if(stop_server(s) < 0) {
		goto err_cmd_failed;
	}

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			pthread_mutex_lock(&db_mutex);
			rc = tup_db_copy_sticky_links(n->tupid, tupid);
			if(rc == 0)
				rc = write_files(tupid, n->tupid, n->dt, name, &s->finfo);
			if(rc == 0) {
				rc = tup_db_yell_links(tupid, "Missing input dependency - a file was read from, and was not specified as an input link for the command. This is an issue because the file was created   from another command, and without the input link the commands may execute out of order. You should add this file as an input, since it is possible this could   randomly break in the future.");
				if(rc == 0) {
					/* Success! Delete the old node */
					delete_name_file(n->tupid);
				}
				/* Yelled nodes are actually a failure */
				if(rc == 1) {
					fprintf(stderr, " -- Command: '%s'\n", name);
					rc = -1;
				}
			}
			pthread_mutex_unlock(&db_mutex);
			if(rc < 0)
				goto err_cmd_failed;
		} else {
			goto err_cmd_failed;
		}
	} else if(WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		const char *errmsg = "Unknown signal";

		if(sig >= 0 && sig < ARRAY_SIZE(signal_err) && signal_err[sig])
			errmsg = signal_err[sig];
		fprintf(stderr, " *** Killed by signal %i (%s)\n", sig, errmsg);
		goto err_cmd_failed;
	} else {
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		goto err_cmd_failed;
	}

	close(dfd);
	return rc;

err_cmd_failed:
	fprintf(stderr, " *** Command %lli failed.\n", n->tupid);
err_close_dfd:
	close(dfd);
err_delete_node:
	pthread_mutex_lock(&db_mutex);
	delete_name_file(tupid);
	pthread_mutex_unlock(&db_mutex);
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
	struct db_node dbn;

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

	/* Make sure the input file also becomes a normal link, so if the
	 * input file changes in the future we'll continue to process the
	 * required parts of the DAG. See t3009.
	 */
	if(tup_db_select_dbn(n->dt, input, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0)
		return -1;
	if(tup_db_create_link(dbn.tupid, n->tupid, TUP_LINK_NORMAL) < 0)
		return -1;

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
			if(tup_db_create_link(varid, n->tupid, TUP_LINK_NORMAL) < 0)
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

static void sighandler(int sig)
{
	if(sig) {}
	fprintf(stderr, " *** Signal caught - waiting for jobs to finish.\n");
	sig_quit = 1;
}

static void show_progress(int sum, int tot, struct node *n)
{
	if(do_show_progress && tot) {
		int a, b;
		const int max = 11;
		const char *equals = "===========";
		const char *hashes = "###########";
		const char *spaces = "           ";
		const char *color = "";
		const char *endcolor = "";
		const char *ident;
		char *name;
		int name_sz = 0;

		if(tot > max) {
			a = sum * max / tot;
			b = max;
			ident = hashes;
		} else {
			a = sum;
			b = tot;
			ident = equals;
		}
		if(n) {
			name = n->name;
			name_sz = strlen(n->name);
			if(name[0] == '^') {
				name++;
				while(*name && *name != ' ') name++;
				name++;
				name_sz = 0;
				while(name[name_sz] && name[name_sz] != '^')
					name_sz++;
			}
			if(n->flags & TUP_FLAGS_DELETE) {
				color = "[35m";
				endcolor = "[0m";
			}
			if(n->type == TUP_NODE_DIR) {
				color = "[33m";
				endcolor = "[0m";
			}
			printf("[%.*s%.*s] %i/%i (%3i%%) %lli: %s%.*s%s\n",
			       a, ident, b-a, spaces,
			       sum, tot, sum*100/tot,
			       n->tupid, color, name_sz, name, endcolor);
		} else {
			printf("[%.*s%.*s] [32m%i/%i (%3i%%)[0m\n",
			       a, ident, b-a, spaces,
			       sum, tot, sum*100/tot);
		}
	}
}
