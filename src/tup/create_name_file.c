#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct id_flags {
	tupid_t tupid;
	int flags;
};

tupid_t create_name_file(const char *path)
{
	return tup_db_create_node(path, TUP_NODE_FILE, TUP_FLAGS_MODIFY);
}

tupid_t create_command_file(const char *cmd)
{
	return tup_db_create_node(cmd, TUP_NODE_CMD, TUP_FLAGS_MODIFY);
}

tupid_t create_dir_file(const char *path)
{
	return tup_db_create_node(path, TUP_NODE_DIR, TUP_FLAGS_CREATE);
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
		if(tup_db_set_node_flags(name, TUP_FLAGS_CREATE) < 0) {
			rc = -1;
		}
out:
		*slash = '/';
	} else {
		if(create_dir_file(".") < 0)
			return -1;
		if(tup_db_set_node_flags(".", TUP_FLAGS_CREATE) < 0)
			return -1;
	}

	return rc;
}
