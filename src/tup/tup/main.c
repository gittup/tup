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
#include "tup/path.h"
#include "tup/entry.h"

static int init(int argc, char **argv);
static int graph_cb(void *arg, struct tup_entry *tent, int style);
static int graph(int argc, char **argv);
/* Testing commands */
static int mlink(int argc, char **argv);
static int node_exists(int argc, char **argv);
static int link_exists(int argc, char **argv);
static int touch(int argc, char **argv);
static int node(int argc, char **argv);
static int rm(int argc, char **argv);
static int varshow(int argc, char **argv);
static int config(int argc, char **argv);
static int fake_mtime(int argc, char **argv);
static int flush(void);
static int ghost_check(void);
static void print_name(const char *s, char c);

static void usage(void);

int main(int argc, char **argv)
{
	int rc = 0;
	const char *cmd;

	if(argc < 2) {
		usage();
		return 1;
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
	} else if(strcmp(argv[1], "stop") == 0) {
		return stop_monitor(TUP_MONITOR_SHUTDOWN);
	}

	if(init_getexecwd(argv[0]) < 0) {
		fprintf(stderr, "Error: Unable to determine tup's "
			"execution directory for shared libs.\n");
		return 1;
	}

	if(tup_init() < 0)
		return 1;

	if(strcmp(argv[1], "--debug-sql") == 0) {
		tup_db_enable_sql_debug();
		argc--;
		argv++;
		if(argc < 2) {
			usage();
			return 1;
		}
	}

	cmd = argv[1];
	argc--;
	argv++;
	if(strcmp(cmd, "monitor") == 0) {
		rc = monitor(argc, argv);
	} else if(strcmp(cmd, "g") == 0) {
		rc = graph(argc, argv);
	} else if(strcmp(cmd, "scan") == 0) {
		int pid;
		pid = monitor_get_pid(0);
		if(pid > 0) {
			fprintf(stderr, "Error: monitor appears to be running as pid %i - not doing scan.\n - Run 'tup stop' if you want to kill the monitor and use scan instead.\n", pid);
			return -1;
		}
		rc = tup_scan();
	} else if(strcmp(cmd, "link") == 0) {
		rc = mlink(argc, argv);
	} else if(strcmp(cmd, "read") == 0) {
		rc = updater(argc, argv, 1);
	} else if(strcmp(cmd, "parse") == 0) {
		rc = updater(argc, argv, 2);
	} else if(strcmp(cmd, "upd") == 0) {
		rc = updater(argc, argv, 0);
	} else if(strcmp(cmd, "todo") == 0) {
		rc = todo(argc, argv);
	} else if(strcmp(cmd, "node_exists") == 0) {
		rc = node_exists(argc, argv);
	} else if(strcmp(cmd, "link_exists") == 0) {
		rc = link_exists(argc, argv);
	} else if(strcmp(cmd, "flags_exists") == 0) {
		rc = tup_db_check_flags(TUP_FLAGS_CREATE | TUP_FLAGS_MODIFY);
	} else if(strcmp(cmd, "create_flags_exists") == 0) {
		rc = tup_db_check_flags(TUP_FLAGS_CREATE);
	} else if(strcmp(cmd, "touch") == 0) {
		rc = touch(argc, argv);
	} else if(strcmp(cmd, "node") == 0) {
		rc = node(argc, argv);
	} else if(strcmp(cmd, "rm") == 0) {
		rc = rm(argc, argv);
	} else if(strcmp(cmd, "varshow") == 0) {
		rc = varshow(argc, argv);
	} else if(strcmp(cmd, "config") == 0) {
		rc = config(argc, argv);
	} else if(strcmp(cmd, "fake_mtime") == 0) {
		rc = fake_mtime(argc, argv);
	} else if(strcmp(cmd, "flush") == 0) {
		rc = flush();
	} else if(strcmp(cmd, "ghost_check") == 0) {
		rc = ghost_check();
	} else if(strcmp(cmd, "monitor_supported") == 0) {
		rc = monitor_supported();
	} else {
		fprintf(stderr, "Unknown tup command: %s\n", cmd);
		rc = 1;
	}

	tup_cleanup();
	return rc;
}

static int init(int argc, char **argv)
{
	int x;
	int db_sync = 1;
	int force_init = 0;
	int fd;

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "--no-sync") == 0) {
			db_sync = 0;
		} else if(strcmp(argv[x], "--force") == 0) {
			/* force should only be used for tup/test */
			force_init = 1;
		}
	}

	fd = open(".", O_RDONLY);
	if(fd < 0) {
		perror(".");
		return -1;
	}

	if(!force_init && find_tup_dir() == 0) {
		char wd[PATH_MAX];
		if(getcwd(wd, sizeof(wd)) == NULL) {
			perror("getcwd");
			fprintf(stderr, "Error: tup database already exists somewhere up the tree.\n");
		} else {
			fprintf(stderr, "Error: tup database already exists in directory: %s\n", wd);
		}
		goto err_close;
	}
	if(fchdir(fd) < 0) {
		perror("fchdir");
		goto err_close;
	}
	close(fd);

	if(mkdir(TUP_DIR, 0777) != 0) {
		perror(TUP_DIR);
		return -1;
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
	if(creat(TUP_VARDICT_FILE, 0666) < 0) {
		perror(TUP_VARDICT_FILE);
		return -1;
	}
	return 0;

err_close:
	close(fd);
	return -1;
}

static int graph_cb(void *arg, struct tup_entry *tent, int style)
{
	struct graph *g = arg;
	struct node *n;

	n = find_node(g, tent->tnode.tupid);
	if(n != NULL)
		goto edge_create;
	n = create_node(g, tent);
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
	tupid_t sub_dir_dt;

	if(create_graph(&g, 0) < 0)
		return -1;

	sub_dir_dt = get_sub_dir_dt();
	if(sub_dir_dt < 0)
		return -1;

	if(argc == 1) {
		if(tup_db_select_node_by_flags(graph_cb, &g, TUP_FLAGS_CREATE) < 0)
			return -1;
		if(tup_db_select_node_by_flags(graph_cb, &g, TUP_FLAGS_MODIFY) < 0)
			return -1;
	}
	for(x=1; x<argc; x++) {
		struct tup_entry *tent;

		tent = get_tent_dt(sub_dir_dt, argv[x]);
		if(!tent) {
			fprintf(stderr, "Unable to find tupid for: '%s'\n", argv[x]);
			return -1;
		}

		n = find_node(&g, tent->tnode.tupid);
		if(n == NULL) {
			n = create_node(&g, tent);
			if(!n)
				return -1;
			n->expanded = 1;
			list_move(&n->list, &g.plist);
		}
	}

	while(!list_empty(&g.plist)) {
		g.cur = list_entry(g.plist.next, struct node, list);
		if(tup_db_select_node_by_link(graph_cb, &g, g.cur->tnode.tupid) < 0)
			return -1;
		list_move(&g.cur->list, &g.node_list);

		tupid = g.cur->tnode.tupid;
		g.cur = NULL;
		if(tup_db_select_node_dir(graph_cb, &g, tupid) < 0)
			return -1;
	}

	printf("digraph G {\n");
	list_for_each_entry(n, &g.node_list, list) {
		int color;
		int fontcolor;
		const char *shape;
		const char *style;
		char *s;
		struct edge *e;
		int flags;

		if(n == g.root)
			continue;

		style = "solid";
		color = 0;
		fontcolor = 0;
		switch(n->tent->type) {
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
			case TUP_NODE_GHOST:
				/* Ghost nodes won't have flags set */
				color = 0x888888;
				fontcolor = 0x888888;
				style = "dotted";
				shape = "oval";
				break;
			default:
				shape="ellipse";
		}

		flags = tup_db_get_node_flags(n->tnode.tupid);
		if(flags & TUP_FLAGS_MODIFY) {
			color |= 0x0000ff;
			style = "dashed";
		}
		if(flags & TUP_FLAGS_CREATE) {
			color |= 0x00ff00;
			style = "dashed peripheries=2";
		}
		if(n->expanded == 0) {
			if(color == 0) {
				color = 0x888888;
				fontcolor = 0x888888;
			} else {
				/* Might only be graphing a subset. Ie:
				 * graph node foo, which points to command bar,
				 * and command bar is in the modify list. In
				 * this case, bar won't be expanded.
				 */
			}
		}
		printf("\tnode_%lli [label=\"", n->tnode.tupid);
		s = n->tent->name.s;
		if(s[0] == '^') {
			s++;
			while(*s && *s != ' ') {
				/* Skip flags (Currently there are none) */
				s++;
			}
			print_name(s, '^');
		} else {
			print_name(s, 0);
		}
		printf("\\n%lli\" shape=\"%s\" color=\"#%06x\" fontcolor=\"#%06x\" style=%s];\n", n->tnode.tupid, shape, color, fontcolor, style);
		if(n->tent->dt) {
			struct node *tmp;
			tmp = find_node(&g, n->tent->dt);
			if(tmp)
				printf("\tnode_%lli -> node_%lli [dir=back color=\"#888888\" arrowtail=odot]\n", n->tnode.tupid, n->tent->dt);
		}
		if(n->tent->sym != -1)
			printf("\tnode_%lli -> node_%lli [dir=back color=\"#00BBBB\" arrowtail=vee]\n", n->tent->sym, n->tnode.tupid);

		e = n->edges;
		while(e) {
			printf("\tnode_%lli -> node_%lli [dir=back,style=\"%s\",arrowtail=\"%s\"]\n", e->dest->tnode.tupid, n->tnode.tupid, (e->style == TUP_LINK_STICKY) ? "dotted" : "solid", (e->style & TUP_LINK_STICKY) ? "normal" : "empty");
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
	tupid_t cmdid;
	struct tup_entry *tent;

	if(argc < 4) {
		fprintf(stderr, "Usage: %s cmd -iread_file -owrite_file\n",
			argv[0]);
		return 1;
	}

	if(tup_db_begin() < 0)
		return -1;
	if(tup_entry_add(DOT_DT, NULL) < 0)
		return -1;
	cmdid = create_command_file(DOT_DT, argv[1]);
	if(cmdid < 0) {
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

		if(tup_db_select_tent(DOT_DT, name+2, &tent) < 0)
			return -1;
		if(!tent)
			return 1;

		if(type == 0) {
			if(tup_db_create_link(tent->tnode.tupid, cmdid, TUP_LINK_NORMAL) < 0)
				return -1;
		} else {
			if(tup_db_create_link(cmdid, tent->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;
		}
	}
	if(tup_db_commit() < 0)
		return -1;

	return 0;
}

static int node_exists(int argc, char **argv)
{
	int x;
	struct tup_entry *tent;
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
		if(tup_db_select_tent(dt, argv[x], &tent) < 0)
			return -1;
		if(!tent)
			return -1;
	}
	return 0;
}

static int link_exists(int argc, char **argv)
{
	struct tup_entry *tenta;
	struct tup_entry *tentb;
	tupid_t dta, dtb;

	if(argc != 5) {
		fprintf(stderr, "Error: link_exists requires two dir/name pairs.\n");
		return -1;
	}
	if(strcmp(argv[1], "0") == 0)
		dta = 0;
	else
		dta = find_dir_tupid(argv[1]);
	if(dta < 0) {
		fprintf(stderr, "[31mError: dir '%s' doesn't exist.[0m\n", argv[1]);
		return -1;
	}

	if(tup_db_select_tent(dta, argv[2], &tenta) < 0)
		return -1;
	if(!tenta) {
		fprintf(stderr, "[31mError: node '%s' doesn't exist.[0m\n", argv[2]);
		return -1;
	}

	if(strcmp(argv[3], "0") == 0)
		dtb = 0;
	else
		dtb = find_dir_tupid(argv[3]);
	if(dtb < 0) {
		fprintf(stderr, "[31mError: dir '%s' doesn't exist.[0m\n", argv[3]);
		return -1;
	}

	if(tup_db_select_tent(dtb, argv[4], &tentb) < 0)
		return -1;
	if(!tentb) {
		fprintf(stderr, "[31mError: node '%s' doesn't exist.[0m\n", argv[4]);
		return -1;
	}
	return tup_db_link_exists(tenta->tnode.tupid, tentb->tnode.tupid);
}

static int touch(int argc, char **argv)
{
	int x;
	tupid_t sub_dir_dt;

	if(tup_db_begin() < 0)
		return -1;
	if(chdir(get_sub_dir()) < 0) {
		perror("chdir");
		return -1;
	}
	sub_dir_dt = get_sub_dir_dt();
	if(sub_dir_dt < 0)
		return -1;

	for(x=1; x<argc; x++) {
		struct stat buf;
		struct path_element *pel = NULL;
		tupid_t dt;

		if(lstat(argv[x], &buf) < 0) {
			close(open(argv[x], O_WRONLY | O_CREAT, 0666));
			if(lstat(argv[x], &buf) < 0) {
				fprintf(stderr, "lstat: ");
				perror(argv[x]);
				return -1;
			}
		}

		dt = find_dir_tupid_dt(sub_dir_dt, argv[x], &pel, NULL, 0);
		if(dt <= 0) {
			fprintf(stderr, "Error finding dt for dir '%s' relative to dir %lli\n", argv[x], sub_dir_dt);
			return -1;
		}
		if(S_ISDIR(buf.st_mode)) {
			if(create_dir_file(dt, pel->path) < 0)
				return -1;
		} else if(S_ISREG(buf.st_mode)) {
			if(tup_file_mod_mtime(dt, pel->path, buf.st_mtime, 1) < 0)
				return -1;
		} else if(S_ISLNK(buf.st_mode)) {
			int fd;
			fd = open(".", O_RDONLY);
			if(fd < 0) {
				perror(".");
				return -1;
			}
			if(update_symlink_fileat(dt, fd, pel->path, buf.st_mtime, 1) < 0)
				return -1;
			close(fd);
		}
		free(pel);
	}
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int node(int argc, char **argv)
{
	int x;
	tupid_t sub_dir_dt;
	sub_dir_dt = get_sub_dir_dt();
	if(sub_dir_dt < 0)
		return -1;

	if(tup_db_begin() < 0)
		return -1;
	for(x=1; x<argc; x++) {
		tupid_t dt;
		struct path_element *pel = NULL;

		dt = find_dir_tupid_dt(sub_dir_dt, argv[x], &pel, NULL, 0);
		if(dt <= 0) {
			fprintf(stderr, "Unable to find dir '%s' relative to %lli\n", argv[x], sub_dir_dt);
			return -1;
		}
		if(create_name_file(dt, pel->path, -1, NULL) < 0) {
			fprintf(stderr, "Unable to create node for '%s' in dir %lli\n", pel->path, dt);
			return -1;
		}
		free(pel);
	}
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int rm(int argc, char **argv)
{
	int x;
	tupid_t sub_dir_dt;
	sub_dir_dt = get_sub_dir_dt();
	if(sub_dir_dt < 0)
		return -1;

	if(tup_db_begin() < 0)
		return -1;
	for(x=1; x<argc; x++) {
		struct path_element *pel = NULL;
		tupid_t dt;

		dt = find_dir_tupid_dt(sub_dir_dt, argv[x], &pel, NULL, 0);
		if(dt < 0) {
			fprintf(stderr, "Unable to find dir '%s' relative to %lli\n", argv[x], sub_dir_dt);
			return -1;
		}
		if(tup_file_del(dt, pel->path, pel->len) < 0)
			return -1;
		free(pel);
	}
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int varshow_cb(void *arg, const char *var, const char *value, int type)
{
	const char *color1 = "";
	const char *color2 = "";
	if(arg) {}
	if(type == TUP_NODE_GHOST) {
		color1 = "[47;30m";
		color2 = "[0m";
	}
	printf(" - Var[%s%s%s] = '%s'\n", color1, var, color2, value);
	return 0;
}

static int varshow(int argc, char **argv)
{
	if(tup_entry_add(VAR_DT, NULL) < 0)
		return -1;
	if(argc == 1) {
		if(tup_db_var_foreach(varshow_cb, NULL) < 0)
			return -1;
	} else {
		int x;
		struct tup_entry *tent;
		for(x=1; x<argc; x++) {
			char *value;
			if(tup_db_select_tent(VAR_DT, argv[x], &tent) < 0)
				return -1;
			if(!tent) {
				fprintf(stderr, "Unable to find tupid for variable '%s'\n", argv[x]);
				continue;
			}
			if(tent->type == TUP_NODE_VAR) {
				if(tup_db_get_var_id_alloc(tent->tnode.tupid, &value) < 0)
					return -1;
				printf(" - Var[%s] = '%s'\n", argv[x], value);
				free(value);
			} else if(tent->type == TUP_NODE_GHOST) {
				printf(" - Var[[47;30m%s[0m] is a ghost\n", argv[x]);
			} else {
				fprintf(stderr, "Variable '%s' has unknown type %i\n", argv[x], tent->type);
			}
		}
	}
	return 0;
}

static int config(int argc, char **argv)
{
	if(argc == 1) {
		if(tup_db_show_config() < 0)
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

static int fake_mtime(int argc, char **argv)
{
	struct tup_entry *tent;
	time_t mtime;
	tupid_t dt;
	tupid_t sub_dir_dt;
	struct path_element *pel = NULL;

	if(argc != 3) {
		fprintf(stderr, "Error: fake_mtime requires a file and an mtime.\n");
		return -1;
	}
	sub_dir_dt = get_sub_dir_dt();
	if(sub_dir_dt < 0)
		return -1;
	dt = find_dir_tupid_dt(sub_dir_dt, argv[1], &pel, NULL, 0);
	if(dt < 0) {
		fprintf(stderr, "Error: Unable to find dt for node: %s\n", argv[1]);
		return -1;
	}
	if(tup_db_select_tent_part(dt, pel->path, pel->len, &tent) < 0) {
		fprintf(stderr, "Unable to find node '%.*s' in dir %lli\n", pel->len, pel->path, dt);
		return -1;
	}
	mtime = strtol(argv[2], NULL, 0);
	if(tup_db_set_mtime(tent, mtime) < 0)
		return -1;
	free(pel);
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

static int ghost_check(void)
{
	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_debug_add_all_ghosts() < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static void print_name(const char *s, char c)
{
	for(; *s && *s != c; s++) {
		if(*s == '"') {
			printf("\\\"");
		} else if(*s == '\\') {
			printf("\\\\");
		} else {
			printf("%c", *s);
		}
	}
}

static void usage(void)
{
	printf("tup %s usage: tup command [args]\n", tup_version());
	printf("Where command is:\n");
	printf("  init		Initialize the tup database in .tup/\n");
	printf("  upd		Update everything according to the Tupfiles\n");
	printf("For information on Tupfiles and other commands, see the tup(1) man page.\n");
}
