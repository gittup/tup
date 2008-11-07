#include "fileio.h"
#include "db.h"
#include <stdio.h>

static int create_node(const char *name, int type, int flags);

int create_name_file(const char *path)
{
	return create_node(path, TUP_NODE_FILE, TUP_FLAGS_MODIFY);
}

int create_command_file(const char *cmd)
{
	return create_node(cmd, TUP_NODE_CMD, TUP_FLAGS_MODIFY);
}

int create_dir_file(const char *path)
{
	return create_node(path, TUP_NODE_DIR, TUP_FLAGS_CREATE);
}

static int create_node(const char *name, int type, int flags)
{
	int rc;
	char *errmsg;

	/* The "or ignore" is supposed to ignore the error if the entry is
	 * already in the database (in that case, we'll just re-use the
	 * existing one). However, it also seems to ignore other errors (like
	 * if a 'not null' column isn't specified, or something), which can be
	 * annoying.
	 */
	rc = tup_db_exec(&errmsg,
			 "insert or ignore into node(name, type, flags) values('%s', %i, %i)",
			 name, type, flags);
	if(rc == 0)
		return 0;

	fprintf(stderr, "SQL node insertion error: %s\n", errmsg);
	return -1;
}
