#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "tup/config.h"
#include "tup/lock.h"
#include "tup/getexecwd.h"
#include "tup/monitor.h"
#include "tup/fileio.h"
#include "tup/updater.h"
#include "tup/graph.h"
#include "tup/init.h"
#include "tup/compat.h"
#include "tup/version.h"

static int file_exists(const char *s);

static int init(int argc, char **argv);
static int graph_cb(void *arg, struct db_node *dbn, int style);
static int graph(int argc, char **argv);
/* Testing commands */
static int mlink(int argc, char **argv);
static int node_exists(int argc, char **argv);
static int link_exists(int argc, char **argv);
static int flags_exists_cb(void *arg, int argc, char **argv, char **col);
static int flags_exists(int argc, char **argv);
static int touch(int argc, char **argv);
static int delete(int argc, char **argv);
static int varset(int argc, char **argv);
static int config_cb(void *arg, int argc, char **argv, char **col);
static int config(int argc, char **argv);
static int flush(void);

static void usage(void);

static int start_fd;

int main(int argc, char **argv)
{
	int rc = 0;
	const char *cmd;

	if(argc < 2) {
		usage();
		return 1;
	}
	for (start_fd = 4; start_fd < (int) FD_SETSIZE; start_fd++) {
		int flags;
		errno = 0;
		flags = fcntl(start_fd, F_GETFD, 0);
		if (flags == -1 && errno) {
			if (errno != EBADF) {
				perror("fcntl");
				return -1;
			}
			else
				break;
		}
	}

	if(strcmp(argv[1], "init") == 0) {
		argc--;
		argv++;
		return init(argc, argv);
	} else if(strcmp(argv[1], "version") == 0 ||
		  strcmp(argv[1], "--version") == 0 ||
		  strcmp(argv[1], "-v") == 0) {
		printf("tup %s\n", tup_version());
		return 0;
	}

	if(init_getexecwd(argv[0]) < 0) {
		fprintf(stderr, "Error: Unable to determine tup's "
			"execution directory for shared libs.\n");
		return 1;
	}

	if(tup_init() < 0)
		return 1;

	cmd = argv[1];
	argc--;
	argv++;
	if(strcmp(cmd, "monitor") == 0) {
		rc = monitor(argc, argv);
	} else if(strcmp(cmd, "stop") == 0) {
		rc = stop_monitor(argc, argv);
	} else if(strcmp(cmd, "g") == 0) {
		rc = graph(argc, argv);
	} else if(strcmp(cmd, "link") == 0) {
		rc = mlink(argc, argv);
	} else if(strcmp(cmd, "parse") == 0) {
		rc = updater(argc, argv, 1);
	} else if(strcmp(cmd, "upd") == 0) {
		rc = updater(argc, argv, 0);
	} else if(strcmp(cmd, "node_exists") == 0) {
		rc = node_exists(argc, argv);
	} else if(strcmp(cmd, "link_exists") == 0) {
		rc = link_exists(argc, argv);
	} else if(strcmp(cmd, "flags_exists") == 0) {
		rc = flags_exists(argc, argv);
	} else if(strcmp(cmd, "touch") == 0) {
		rc = touch(argc, argv);
	} else if(strcmp(cmd, "delete") == 0) {
		rc = delete(argc, argv);
	} else if(strcmp(cmd, "varset") == 0) {
		rc = varset(argc, argv);
	} else if(strcmp(cmd, "config") == 0) {
		rc = config(argc, argv);
	} else if(strcmp(cmd, "flush") == 0) {
		rc = flush();
	} else if(strcmp(cmd, "kconfig_pre_delete") == 0) {
		rc = tup_db_or_dircmd_flags(VAR_DT, TUP_FLAGS_DELETE, TUP_NODE_VAR);
	} else if(strcmp(cmd, "kconfig_post_delete") == 0) {
		rc = tup_db_flag_deleted_var_dependent_dirs();
	} else {
		fprintf(stderr, "Unknown tup command: %s\n", argv[0]);
		rc = 1;
	}

	tup_cleanup();
	return rc;
}

static int file_exists(const char *s)
{
	struct stat buf;

	if(stat(s, &buf) == 0) {
		return 1;
	}
	return 0;
}

static int init(int argc, char **argv)
{
	int x;
	int db_sync = 1;

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "--no-sync") == 0)
			db_sync = 0;
	}

	if(file_exists(TUP_DB_FILE)) {
		printf("TODO: DB file already exists. abort\n");
		return -1;
	}

	if(!file_exists(TUP_DIR)) {
		if(mkdir(TUP_DIR, 0777) != 0) {
			perror(TUP_DIR);
			return -1;
		}
	}

	if(tup_db_create(db_sync) != 0) {
		return -1;
	}

	if(creat(TUP_OBJECT_LOCK, 0666) < 0) {
		perror(TUP_OBJECT_LOCK);
		return -1;
	}
	if(creat(TUP_SHARED_LOCK, 0666) < 0) {
		perror(TUP_SHARED_LOCK);
		return -1;
	}
	if(creat(TUP_TRI_LOCK, 0666) < 0) {
		perror(TUP_TRI_LOCK);
		return -1;
	}
	if(creat(TUP_MONITOR_LOCK, 0666) < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}
	return 0;
}

static int graph_cb(void *arg, struct db_node *dbn, int style)
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

edge_create:
	if(style & TUP_LINK_NORMAL && n->expanded == 0) {
		n->expanded = 1;
		list_move(&n->list, &g->plist);
	}
	if(g->cur)
		if(create_edge(g->cur, n, style) < 0)
			return -1;
	return 0;
}

static int graph(int argc, char **argv)
{
	int x;
	struct graph g;
	struct node *n;
	tupid_t tupid;

	if(create_graph(&g, 0) < 0)
		return -1;

	if(argc == 1) {
		if(tup_db_select_node_by_flags(graph_cb, &g, TUP_FLAGS_CREATE) < 0)
			return -1;
		if(tup_db_select_node_by_flags(graph_cb, &g, TUP_FLAGS_MODIFY) < 0)
			return -1;
		if(tup_db_select_node_by_flags(graph_cb, &g, TUP_FLAGS_DELETE) < 0)
			return -1;
	}
	for(x=1; x<argc; x++) {
		struct db_node dbn;
		int len;
		static char cname[PATH_MAX];

		len = canonicalize(argv[x], cname, sizeof(cname), NULL,
				   get_sub_dir());
		if(len < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n", argv[x]);
			return -1;
		}
		tupid = get_dbn(cname, &dbn);
		if(tupid < 0) {
			fprintf(stderr, "Unable to find tupid for: '%s'\n", argv[x]);
			return -1;
		}

		if(find_node(&g, dbn.tupid, &n) < 0)
			return -1;
		if(n == NULL) {
			n = create_node(&g, &dbn);
			if(!n)
				return -1;
			n->expanded = 1;
			list_move(&n->list, &g.plist);
		}
	}

	while(!list_empty(&g.plist)) {
		g.cur = list_entry(g.plist.next, struct node, list);
		if(tup_db_select_node_by_link(graph_cb, &g, g.cur->tupid) < 0)
			return -1;
		list_move(&g.cur->list, &g.node_list);

		if(g.cur->type == TUP_NODE_DIR) {
			tupid = g.cur->tupid;
			g.cur = NULL;
			if(tup_db_select_node_dir(graph_cb, &g, tupid) < 0)
				return -1;
		}
	}

	printf("digraph G {\n");
	list_for_each_entry(n, &g.node_list, list) {
		int color;
		int fontcolor;
		const char *shape;
		const char *style;
		char *s;
		struct edge *e;

		if(n == g.root)
			continue;

		switch(n->type) {
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
			case TUP_NODE_VAR:
				shape = "octagon";
				break;
			default:
				shape="ellipse";
		}

		style = "solid";
		color = 0;
		fontcolor = 0;
		if(n->flags & TUP_FLAGS_DELETE) {
			color |= 0xff0000;
			style = "dotted";
		}
		if(n->flags & TUP_FLAGS_MODIFY) {
			color |= 0x0000ff;
			style = "dashed";
		}
		if(n->flags & TUP_FLAGS_CREATE) {
			color |= 0x00ff00;
			style = "dashed peripheries=2";
		}
		if(n->expanded == 0) {
			if(color == 0) {
				color = 0x888888;
				fontcolor = 0x888888;
			} else {
				fprintf(stderr, "tup error: How is color non-zero, but the node isn't expanded? Node is %lli\n", n->tupid);
				return -1;
			}
		}
		printf("\tnode_%lli [label=\"", n->tupid);
		for(s = n->name; *s; s++) {
			if(*s == '"') {
				printf("\\\"");
			} else if(*s == '\\') {
				printf("\\\\");
			} else {
				printf("%c", *s);
			}
		}
		printf("\\n%lli\" shape=\"%s\" color=\"#%06x\" fontcolor=\"#%06x\" style=%s];\n", n->tupid, shape, color, fontcolor, style);
		if(n->dt)
			printf("\tnode_%lli -> node_%lli [dir=back color=\"#888888\"]\n", n->tupid, n->dt);

		e = n->edges;
		while(e) {
			printf("\tnode_%lli -> node_%lli [dir=back,style=\"%s\"]\n", e->dest->tupid, n->tupid, (e->style == TUP_LINK_STICKY) ? "dotted" : "solid");
			e = e->next;
		}
	}
	printf("}\n");
	destroy_graph(&g);
	return 0;
}

static int mlink(int argc, char **argv)
{
	/* This only works for files in the top-level directory. It's only
	 * used by the benchmarking suite, and in fact may just disappear
	 * entirely. I wouldn't use it for any other purpose.
	 */
	int type;
	int x;
	tupid_t cmd_id;
	tupid_t dotdt;
	tupid_t id;

	if(argc < 4) {
		fprintf(stderr, "Usage: %s cmd -iread_file -owrite_file\n",
			argv[0]);
		return 1;
	}

	dotdt = create_dir_file(0, ".");
	if(dotdt < 0)
		return -1;

	cmd_id = create_command_file(dotdt, argv[1]);
	if(cmd_id < 0) {
		return -1;
	}

	for(x=2; x<argc; x++) {
		char *name = argv[x];
		if(name[0] == '-') {
			if(name[1] == 'i') {
				type = 0;
			} else if(name[1] == 'o') {
				type = 1;
			} else {
				fprintf(stderr, "Invalid argument: '%s'\n",
					name);
				return 1;
			}
		} else {
			fprintf(stderr, "Invalid argument: '%s'\n", name);
			return 1;
		}

		id = create_name_file(dotdt, name+2);
		if(id < 0)
			return 1;

		if(type == 0) {
			if(tup_db_create_link(id, cmd_id, TUP_LINK_NORMAL) < 0)
				return -1;
		} else {
			if(tup_db_create_link(cmd_id, id, TUP_LINK_NORMAL) < 0)
				return -1;
		}
	}

	return 0;
}

static int node_exists(int argc, char **argv)
{
	int x;
	tupid_t dt;

	if(argc < 3) {
		fprintf(stderr, "Usage: node_exists dir [n1] [n2...]\n");
		return -1;
	}
	dt = find_dir_tupid(argv[1]);
	if(dt < 0)
		return -1;
	argv++;
	argc--;
	for(x=1; x<argc; x++) {
		if(tup_db_select_node(dt, argv[x]) < 0)
			return -1;
	}
	return 0;
}

static int link_exists(int argc, char **argv)
{
	tupid_t dta, dtb;
	tupid_t a, b;

	if(argc != 5) {
		fprintf(stderr, "Error: link_exists requires two dir/name pairs.\n");
		return -1;
	}
	if(strcmp(argv[1], "0") == 0)
		dta = 0;
	else
		dta = find_dir_tupid(argv[1]);
	if(dta < 0) {
		fprintf(stderr, "Error: dir '%s' doesn't exist.\n", argv[1]);
		return -1;
	}

	a = tup_db_select_node(dta, argv[2]);
	if(a < 0) {
		fprintf(stderr, "Error: node '%s' doesn't exist.\n", argv[2]);
		return -1;
	}

	if(strcmp(argv[3], "0") == 0)
		dtb = 0;
	else
		dtb = find_dir_tupid(argv[3]);
	if(dtb < 0) {
		fprintf(stderr, "Error: dir '%s' doesn't exist.\n", argv[3]);
		return -1;
	}

	b = tup_db_select_node(dtb, argv[4]);
	if(b < 0) {
		fprintf(stderr, "Error: node '%s' doesn't exist.\n", argv[4]);
		return -1;
	}
	return tup_db_link_exists(a, b);
}

static int flags_exists_cb(void *arg, int argc, char **argv, char **col)
{
	int *iptr = arg;
	if(argc) {}
	if(argv) {}
	if(col) {}

	*iptr = 1;

	return 0;
}

static int flags_exists(int argc, char **argv)
{
	int x = 0;
	if(argc) {}
	if(argv) {}

	if(tup_db_select(flags_exists_cb, &x,
			 "select * from create_list") != 0)
		return -1;
	if(tup_db_select(flags_exists_cb, &x,
			 "select * from modify_list") != 0)
		return -1;
	if(tup_db_select(flags_exists_cb, &x,
			 "select * from delete_list") != 0)
		return -1;
	return x;
}

static int touch(int argc, char **argv)
{
	int x;
	int fd;
	if(tup_db_begin() < 0)
		return -1;
	fd = open(".", O_RDONLY);
	if(fd < 0) {
		perror(".");
		return -1;
	}
	for(x=1; x<argc; x++) {
		chdir(get_sub_dir());
		close(open(argv[x], O_WRONLY | O_CREAT, 0666));
		fchdir(fd);
		if(tup_pathname_mod(argv[x], TUP_FLAGS_MODIFY) < 0)
			return -1;
	}
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int delete(int argc, char **argv)
{
	int x;
	for(x=1; x<argc; x++) {
		if(tup_pathname_mod(argv[x], TUP_FLAGS_DELETE) < 0)
			return -1;
	}
	return 0;
}

static int varset(int argc, char **argv)
{
	if(argc != 3) {
		fprintf(stderr, "Error: varset requires exactly two args\n");
		return -1;
	}
	if(create_var_file(argv[1], argv[2]) < 0)
		return -1;
	return 0;
}

static int config_cb(void *arg, int argc, char **argv, char **col)
{
	int x;
	char *lval = NULL;
	char *rval = NULL;
	if(arg) {/* unused */}

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "lval") == 0)
			lval = argv[x];
		if(strcmp(col[x], "rval") == 0)
			rval = argv[x];
	}
	printf("%s: '%s'\n", lval, rval);
	return 0;
}

static int config(int argc, char **argv)
{
	if(argc == 1) {
		if(tup_db_select(config_cb, NULL, "select * from config") != 0)
			return -1;
	} else if(argc == 3) {
		if(tup_db_config_set_string(argv[1], argv[2]) < 0)
			return -1;
	} else {
		fprintf(stderr, "Error: config requires either 0 or 2 arguments.\n");
		return -1;
	}
	return 0;
}

static int flush(void)
{
	struct timespec ts = {0, 10000000};
	printf("Flush\n");
	while(tup_db_config_get_int(AUTOUPDATE_PID) > 0) {
		printf(" -- flush (try again)\n");
		/* If we got the lock but autoupdate pid was set, it must've
		 * just started but not gotten the lock yet.  So we need to
		 * release our lock and wait a bit.
		 */
		tup_cleanup();
		nanosleep(&ts, NULL);
		tup_init();
	}
	printf("Flushed.\n");
	return 0;
}

static void usage(void)
{
	printf("tup %s usage: tup command [args]\n", tup_version());
	printf("Where command is:\n");
	printf("  init		Initialize the tup database in .tup/\n");
	printf("  monitor 	Start the file monitor\n");
	printf("  stop		Stop the file monitor\n");
	printf("  g		Print a graphviz .dot graph of the .tup repository to stdout\n");
	printf("  upd		Run the updater. (Actually build stuff).\n");
	printf("  config        Display or set configuration options.\n");
}
