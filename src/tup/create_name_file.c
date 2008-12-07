#include "fileio.h"
#include "db.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct id_flags {
	tupid_t tupid;
	int flags;
};

tupid_t create_name_file(tupid_t dt, const char *path)
{
	return tup_db_create_node(dt, path, TUP_NODE_FILE, TUP_FLAGS_MODIFY);
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	return tup_db_create_node(dt, cmd, TUP_NODE_CMD, TUP_FLAGS_MODIFY);
}

tupid_t create_dir_file(const char *path)
{
	return tup_db_create_node(0, path, TUP_NODE_DIR, TUP_FLAGS_CREATE);
}

int tup_file_mod(tupid_t dt, const char *file, int flags)
{
	int upddir = 0;
	tupid_t tupid;

	/* Tried to simplify the gross if logic. Basically we want to re-update
	 * the create nodes if:
	 * 1) the file is new
	 * 2) the file was deleted
	 * 3) a Tupfile was modified
	 */
	if(tup_db_select_node(dt, file) < 0)
		upddir = 1;
	if(flags == TUP_FLAGS_DELETE)
		upddir = 1;
	if(strcmp(file, "Tupfile") == 0)
		upddir = 1;

	if(upddir)
		if(tup_db_set_flags_by_id(dt, TUP_FLAGS_CREATE) < 0)
			return -1;

	tupid = create_name_file(dt, file);
	if(tupid < 0)
		return -1;
	if(tup_db_set_flags_by_id(tupid, flags) < 0)
		return -1;

	return 0;
}

int tup_pathname_mod(const char *path, int flags)
{
	static char cname[PATH_MAX];
	int len;
	int lastslash;
	char *name;
	tupid_t dt;

	len = canonicalize(path, cname, sizeof(cname), &lastslash);
	if(len < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", path);
		return -1;
	}
	if(lastslash == -1) {
		dt = create_dir_file(".");
		if(dt < 0)
			return -1;
		name = cname;
	} else {
		cname[lastslash] = 0;
		dt = create_dir_file(cname);
		if(dt < 0)
			return -1;
		name = &cname[lastslash+1];
	}

	tup_file_mod(dt, name, flags);

	return 0;
}
