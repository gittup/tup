#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tup/config.h"
#include "tup/compat.h"
#include "tup/db.h"
#include "tup/lock.h"
#include "tup/getexecwd.h"
#include "tup/monitor.h"
#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/updater.h"
#include "tup/wrap.h"

#define TUP_DIR ".tup"

static int file_exists(const char *s);

static int init(int argc, char **argv);
static int graph_node_cb(void *unused, int argc, char **argv, char **col);
static int graph_link_cb(void *unused, int argc, char **argv, char **col);
static int graph(int argc, char **argv);
static int mlink(int argc, char **argv);
/* Testing commands */
static int node_exists(int argc, char **argv);
static int link_exists(int argc, char **argv);
static int flags_exists_cb(void *arg, int argc, char **argv, char **col);
static int flags_exists(int argc, char **argv);
static int file_mod(const char *file, int flags);
static int get_flags_cb(void *arg, int argc, char **argv, char **col);
static int get_flags(int argc, char **argv);
static int touch(int argc, char **argv);
static int delete(int argc, char **argv);

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
	}

	if(find_tup_dir() != 0) {
		return 1;
	}

	if(init_getexecwd(argv[0]) < 0) {
		fprintf(stderr, "Error: Unable to determine tup's "
			"execution directory for shared libs.\n");
		return 1;
	}

	if(tup_lock_init() < 0) {
		return 1;
	}
	if(tup_db_open() != 0) {
		rc = 1;
		goto out;
	}

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
	} else if(strcmp(cmd, "upd") == 0) {
		rc = updater(argc, argv);
	} else if(strcmp(cmd, "wrap") == 0) {
		rc = wrap(argc, argv);
	} else if(strcmp(cmd, "node_exists") == 0) {
		rc = node_exists(argc, argv);
	} else if(strcmp(cmd, "link_exists") == 0) {
		rc = link_exists(argc, argv);
	} else if(strcmp(cmd, "flags_exists") == 0) {
		rc = flags_exists(argc, argv);
	} else if(strcmp(cmd, "get_flags") == 0) {
		rc = get_flags(argc, argv);
	} else if(strcmp(cmd, "touch") == 0) {
		rc = touch(argc, argv);
	} else if(strcmp(cmd, "delete") == 0) {
		rc = delete(argc, argv);
	} else {
		fprintf(stderr, "Unknown tup command: %s\n", argv[0]);
		rc = 1;
	}

out:
	tup_lock_exit();
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
	if(creat(TUP_UPDATE_LOCK, 0666) < 0) {
		perror(TUP_UPDATE_LOCK);
		return -1;
	}
	if(creat(TUP_MONITOR_LOCK, 0666) < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}
	return 0;
}

static int graph_node_cb(void *unused, int argc, char **argv, char **col)
{
	int x;
	int id = 0;
	char *name = NULL;
	int type = 0;
	int flags = 0;
	int color;
	const char *shape;

	if(unused) {}

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "id") == 0) {
			id = atoll(argv[x]);
		} else if(strcmp(col[x], "name") == 0) {
			name = argv[x];
		} else if(strcmp(col[x], "type") == 0) {
			type = atoll(argv[x]);
		} else if(strcmp(col[x], "flags") == 0) {
			flags = atoll(argv[x]);
		}
	}

	switch(type) {
		case TUP_NODE_FILE:
			shape = "oval";
			break;
		case TUP_NODE_CMD:
			shape = "rectangle";
			break;
		case TUP_NODE_DIR:
			shape = "diamond";
			break;
		default:
			shape="ellipse";
	}

	color = 0;
	if(flags & TUP_FLAGS_MODIFY)
		color |= 0x0000ff;
	if(flags & TUP_FLAGS_CREATE)
		color |= 0x00ff00;
	if(flags & TUP_FLAGS_DELETE)
		color |= 0xff0000;
	printf("\tnode_%i [label=\"%s\" shape=\"%s\" color=\"#%06x\"];\n", id, name, shape, color);

	return 0;
}

static int graph_link_cb(void *unused, int argc, char **argv, char **col)
{
	int x;
	int from_id = 0;
	int to_id = 0;

	if(unused) {}

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "from_id") == 0) {
			from_id = atoll(argv[x]);
		} else if(strcmp(col[x], "to_id") == 0) {
			to_id = atoll(argv[x]);
		}
	}

	printf("\tnode_%i -> node_%i [dir=back]\n", to_id, from_id);
	return 0;
}

static int graph(int argc, char **argv)
{
	const char node_sql[] = "select * from node";
	const char link_sql[] = "select * from link";
	const char cmdlink_sql[] = "select * from cmdlink";

	if(argc) {}
	if(argv) {}

	printf("digraph G {\n");
	if(tup_db_select(graph_node_cb, NULL, node_sql) != 0)
		return -1;

	if(tup_db_select(graph_link_cb, NULL, link_sql) != 0)
		return -1;

	if(tup_db_select(graph_link_cb, NULL, cmdlink_sql) != 0)
		return -1;

	printf("}\n");
	return 0;
}

static int mlink(int argc, char **argv)
{
	static char cname[PATH_MAX];
	int type;
	int x;
	tupid_t cmd_id;
	tupid_t id;

	if(argc < 4) {
		fprintf(stderr, "Usage: %s cmd -iread_file -owrite_file\n",
			argv[0]);
		return 1;
	}

	cmd_id = create_command_file(argv[1]);
	if(cmd_id < 0) {
		return -1;
	}

	id = create_dir_file(get_sub_dir());
	if(id < 0)
		return -1;
	if(tup_db_create_cmdlink(id, cmd_id) < 0)
		return -1;

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
		if(canonicalize(name+2, cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n", argv[2]);
			return 1;
		}

		id = create_name_file(cname);
		if(id < 0)
			return 1;

		if(type == 0) {
			if(tup_db_create_link(id, cmd_id) < 0)
				return -1;
		} else {
			if(tup_db_create_link(cmd_id, id) < 0)
				return -1;
		}
	}

	return 0;
}

static int node_exists(int argc, char **argv)
{
	int x;
	for(x=1; x<argc; x++) {
		if(tup_db_select_node(argv[x]) < 0)
			return -1;
	}
	return 0;
}

static int link_exists(int argc, char **argv)
{
	tupid_t a, b;

	if(argc != 3) {
		fprintf(stderr, "Error: link_exists requires two filenames\n");
		return -1;
	}
	a = tup_db_select_node(argv[1]);
	if(a < 0) {
		fprintf(stderr, "Error: node '%s' doesn't exist.\n", argv[1]);
		return -1;
	}
	b = tup_db_select_node(argv[2]);
	if(b < 0) {
		fprintf(stderr, "Error: node '%s' doesn't exist.\n", argv[1]);
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
			 "select id from node where flags != 0") != 0)
		return -1;
	return x;
}

static int get_flags_cb(void *arg, int argc, char **argv, char **col)
{
	int *iptr = arg;
	int x;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "flags") == 0) {
			*iptr = atoi(argv[x]);
			return 0;
		}
	}
	return -1;
}

static int get_flags(int argc, char **argv)
{
	int flags;
	int requested_flags;

	if(argc != 3) {
		fprintf(stderr, "Error: get_flags requires exactly two args\n");
		return -1;
	}

	if(tup_db_select(get_flags_cb, &flags, "select flags from node where name='%q'", argv[1]) != 0)
		return -1;

	requested_flags = atoi(argv[2]);

	if((flags & requested_flags) != requested_flags)
		return -1;
	return 0;
}

static int file_mod(const char *file, int flags)
{
	static char cname[PATH_MAX];
	static char slash_tup[] = "/Tupfile";
	int len;
	int upddir = 0;
	tupid_t tupid;

	len = canonicalize(file, cname, sizeof(cname));
	if(len < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", file);
		return -1;
	}

	/* Tried to simplify the gross if logic. Basically we want to re-update
	 * the create nodes if:
	 * 1) the file is new
	 * 2) the file was deleted
	 * 3-4) a Tupfile was modified
	 */
	if(tup_db_select_node(cname) < 0)
		upddir = 1;
	if(flags == TUP_FLAGS_DELETE)
		upddir = 1;
	if(len >= (signed)sizeof(slash_tup) &&
	   strcmp(cname + len - sizeof(slash_tup) + 1, slash_tup) == 0)
		upddir = 1;
	if(strcmp(cname, "Tupfile") == 0)
		upddir = 1;

	if(upddir)
		update_create_dir_for_file(cname);

	tupid = create_name_file(cname);
	if(tupid < 0)
		return -1;
	if(tup_db_set_flags_by_id(tupid, flags) < 0)
		return -1;

	return 0;
}

static int touch(int argc, char **argv)
{
	int x;
	if(tup_db_begin() < 0)
		return -1;
	for(x=1; x<argc; x++) {
		if(file_mod(argv[x], TUP_FLAGS_MODIFY) < 0)
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
		if(file_mod(argv[x], TUP_FLAGS_DELETE) < 0)
			return -1;
	}
	return 0;
}

static void usage(void)
{
	printf("Usage: tup command [args]\n");
	printf("Where command is:\n");
	printf("  init		Initialize the tup database in .tup/\n");
	printf("  monitor 	Start the file monitor\n");
	printf("  stop		Stop the file monitor\n");
	printf("  g		Print a graphviz .dot graph of the .tup repository to stdout\n");
	printf("  link cmd -iinfile... -ooutfile...\n\t\tCreate a command node containing 'cmd', with -i as input nodes\n\t\tand -o as output nodes\n");
	printf("  wrap cmd	Run the specified command using the wrapper.\n");
	printf("  upd		Run the updater. (Actually build stuff).\n");
}
