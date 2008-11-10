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

#define ARRAY_SIZE(n) ((signed)(sizeof(n) / sizeof(n[0])))

#define TUP_DIR ".tup"

static int file_exists(const char *s);

static int init(void);
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
		return init();
	}

	if(find_tup_dir() != 0) {
		fprintf(stderr, "Unable to find .tup directory. Run 'tup init' from the top of your working tree first.\n");
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
	if(tup_open_db() != 0) {
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

static int init(void)
{
	int rc;
	int x;
	const char *init_sql[] = {
		"create table node (id integer primary key not null, name varchar(4096) unique, type integer not null, flags integer not null)",
		"create table link (from_id integer, to_id integer)",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		/* TODO: Not needed because name is unique? */
		/*"create index node_index on node(name)",*/
		"create index node_flags_index on node(flags)",
		"create index link_index on link(from_id)",
		"create index link_index2 on link(to_id)"
	};
	char *errmsg;

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

	if(tup_create_db() != 0) {
		return -1;
	}

	for(x=0; x<ARRAY_SIZE(init_sql); x++) {
		rc = sqlite3_exec(tup_db, init_sql[x], NULL, NULL, &errmsg);
		if(rc != 0) {
			fprintf(stderr, "SQL error: %s\n", errmsg);
			return -1;
		}
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
	int id;
	char *name;
	int type;
	int flags;
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
	int from_id;
	int to_id;

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
	char *errmsg;
	int rc;

	if(argc) {}
	if(argv) {}

	printf("digraph G {\n");
	rc = sqlite3_exec(tup_db, node_sql, graph_node_cb, NULL, &errmsg);
	if(rc != 0) {
		fprintf(stderr, "SQL error: %s\n", errmsg);
		return -1;
	}

	rc = sqlite3_exec(tup_db, link_sql, graph_link_cb, NULL, &errmsg);
	if(rc != 0) {
		fprintf(stderr, "SQL error: %s\n", errmsg);
		return -1;
	}
	printf("}\n");
	return 0;
}

static int mlink(int argc, char **argv)
{
	static char cname[PATH_MAX];
	int type;
	int x;
	new_tupid_t cmd_id;
	new_tupid_t id;

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
	if(create_link(id, cmd_id) < 0)
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
			if(create_link(id, cmd_id) < 0)
				return -1;
		} else {
			if(create_link(cmd_id, id) < 0)
				return -1;
		}
	}

	return 0;
}

static int node_exists(int argc, char **argv)
{
	int x;
	for(x=1; x<argc; x++) {
		if(select_node(argv[x]) < 0)
			return -1;
	}
	return 0;
}

static int link_exists(int argc, char **argv)
{
	if(argc != 3) {
		fprintf(stderr, "Error: link_exists requires two filenames\n");
		return -1;
	}
	return find_link(argv[1], argv[2]);
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

	if(canonicalize(file, cname, sizeof(cname)) < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", file);
		return -1;
	}
	if(create_name_file(cname) < 0)
		return -1;
	if(update_node_flags(cname, flags) < 0)
		return -1;

	return 0;
}

static int touch(int argc, char **argv)
{
	int x;
	for(x=1; x<argc; x++) {
		if(file_mod(argv[x], TUP_FLAGS_MODIFY) < 0)
			return -1;
	}
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
}
