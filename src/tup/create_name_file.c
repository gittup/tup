/* For atoll */
#define _ISOC99_SOURCE
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>

static int node_cb(void *id, int argc, char **argv, char **col);
static new_tupid_t create_node(const char *name, int type, int flags);

new_tupid_t create_name_file(const char *path)
{
	return create_node(path, TUP_NODE_FILE, TUP_FLAGS_MODIFY);
}

new_tupid_t create_command_file(const char *cmd)
{
	return create_node(cmd, TUP_NODE_CMD, TUP_FLAGS_MODIFY);
}

new_tupid_t create_dir_file(const char *path)
{
	return create_node(path, TUP_NODE_DIR, TUP_FLAGS_CREATE);
}

static int node_cb(void *id, int argc, char **argv, char **col)
{
	int x;
	new_tupid_t *iptr = id;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "id") == 0) {
			*iptr = atoll(argv[x]);
			return 0;
		}
	}
	return -1;
}

static new_tupid_t create_node(const char *name, int type, int flags)
{
	new_tupid_t id = -1;
	int rc;

	rc = tup_db_select(node_cb, &id,
			   "select id from node where name='%q'", name);
	if(rc == 0 && id != -1) {
		return id;
	}

	rc = tup_db_exec("insert into node(name, type, flags) values('%q', %i, %i)",
			 name, type, flags);
	if(rc == 0)
		return sqlite3_last_insert_rowid(tup_db);
	return -1;
}
