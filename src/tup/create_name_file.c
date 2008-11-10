/* For atoll */
#define _ISOC99_SOURCE
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>

static int node_cb(void *arg, int argc, char **argv, char **col);
static new_tupid_t create_node(const char *name, int type, int flags);

struct id_flags {
	new_tupid_t tupid;
	int flags;
};

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

static int node_cb(void *arg, int argc, char **argv, char **col)
{
	int x;
	struct id_flags *idf = arg;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "id") == 0) {
			idf->tupid = atoll(argv[x]);
		} else if(strcmp(col[x], "flags") == 0) {
			idf->flags = atoi(argv[x]);
		}
	}
	return 0;
}

static new_tupid_t create_node(const char *name, int type, int flags)
{
	struct id_flags idf = {-1, 0};
	int rc;

	rc = tup_db_select(node_cb, &idf,
			   "select id, flags from node where name='%q'", name);
	if(rc == 0 && idf.tupid != -1) {
		/* If the node already exists, just wipe its delete flag */
		if(idf.flags & TUP_FLAGS_DELETE) {
			idf.flags &= ~TUP_FLAGS_DELETE;
			if(tup_db_exec("update node set flags=%i where id=%lli", idf.flags, idf.tupid) < 0)
				return -1;
		}
		return idf.tupid;
	}

	rc = tup_db_exec("insert into node(name, type, flags) values('%q', %i, %i)",
			 name, type, flags);
	if(rc == 0)
		return sqlite3_last_insert_rowid(tup_db);
	return -1;
}

int update_node_flags(const char *name, int flags)
{
	struct id_flags idf = {-1, 0};
	int rc;

	rc = tup_db_select(node_cb, &idf,
			   "select id, flags from node where name='%q'", name);
	if(rc == 0 && idf.tupid != -1) {
		idf.flags |= flags;
		rc = tup_db_exec("update node set flags=%i where id=%lli",
				 idf.flags, idf.tupid);
		if(rc == 0)
			return 0;
	} else {
		fprintf(stderr, "Error: Expected node '%s' to be available, but wasn't.\n", name);
	}
	return -1;
}

new_tupid_t select_node(const char *name)
{
	struct id_flags idf = {-1, 0};
	int rc;

	rc = tup_db_select(node_cb, &idf,
			   "select id from node where name='%q'", name);
	if(rc == 0 && idf.tupid != -1)
		return idf.tupid;
	return -1;
}

int update_create_dir_for_file(char *name)
{
	int rc = 0;
	char *slash;

	slash = strrchr(name, '/');
	if(slash) {
		*slash = 0;
		if(create_dir_file(name) < 0) {
			rc = -1;
			goto out;
		}
		if(update_node_flags(name, TUP_FLAGS_CREATE) < 0) {
			rc = -1;
		}
out:
		*slash = '/';
	} else {
		if(create_dir_file(".") < 0)
			return -1;
		if(update_node_flags(".", TUP_FLAGS_CREATE) < 0)
			return -1;
	}

	return rc;
}
