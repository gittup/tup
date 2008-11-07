#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tup/config.h"
#include "tup/compat.h"
#include "tup/db.h"
#include "tup/monitor.h"

#define ARRAY_SIZE(n) ((signed)(sizeof(n) / sizeof(n[0])))

#define TUP_DIR ".tup"

static int file_exists(const char *s);

static int init(void);
static int graph_node_cb(void *unused, int argc, char **argv, char **col);
static int graph_link_cb(void *unused, int argc, char **argv, char **col);
static int graph(int argc, char **argv);
static void usage(void);

int main(int argc, char **argv)
{
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

	if(tup_open_db() != 0) {
		return 1;
	}

	cmd = argv[1];
	argc--;
	argv++;
	if(strcmp(cmd, "monitor") == 0) {
		return monitor(argc, argv);
	} else if(strcmp(cmd, "g") == 0) {
		return graph(argc, argv);
	} else {
		fprintf(stderr, "Unknown command: %s\n", argv[1]);
		return 1;
	}
	return 0;
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
		"create index node_index on node(name)",
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
	return 0;
}

static int graph_node_cb(void *unused, int argc, char **argv, char **col)
{
	int x;
	int id;
	char *name;
	int type;
	int flags;
	const char *color;
	const char *shape;

	if(unused) {}

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "id") == 0) {
			id = atoi(argv[x]);
		} else if(strcmp(col[x], "name") == 0) {
			name = argv[x];
		} else if(strcmp(col[x], "type") == 0) {
			type = atoi(argv[x]);
		} else if(strcmp(col[x], "flags") == 0) {
			flags = atoi(argv[x]);
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

	switch(flags) {
		case TUP_FLAGS_MODIFY:
			color = "#0000ff";
			break;
		case TUP_FLAGS_CREATE:
			color = "#00ff00";
			break;
		case TUP_FLAGS_DELETE:
			color = "#ff0000";
			break;
		default:
			color = "#000000";
	}
	printf("\tnode_%i [label=\"%s\" shape=\"%s\" color=\"%s\"];\n", id, name, shape, color);

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
			from_id = atoi(argv[x]);
		} else if(strcmp(col[x], "to_id") == 0) {
			to_id = atoi(argv[x]);
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

static void usage(void)
{
	printf("Usage: tup command [args]\n");
	printf("Where command is:\n");
	printf("  init		Initialize the tup database in .tup/\n");
	printf("  monitor 	Start the file monitor\n");
}
