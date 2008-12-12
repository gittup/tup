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

static tupid_t __find_dir_tupid(const char *dir, const char **last);
static tupid_t __create_dir_tupid(const char *dir, int include_last,
				  const char **last_part);

tupid_t create_path_file(const char *path)
{
	const char *file = NULL;
	tupid_t dt;

	dt = __create_dir_tupid(path, 0, &file);
	if(dt < 0)
		return -1;

	return create_name_file(dt, file);
}

tupid_t create_name_file(tupid_t dt, const char *file)
{
	return tup_db_create_node(dt, file, TUP_NODE_FILE, TUP_FLAGS_MODIFY);
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	return tup_db_create_node(dt, cmd, TUP_NODE_CMD, TUP_FLAGS_MODIFY);
}

tupid_t create_dir_file(tupid_t dt, const char *path)
{
	return tup_db_create_node(dt, path, TUP_NODE_DIR, TUP_FLAGS_CREATE);
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

	if(upddir) {
		if(tup_db_set_flags_by_id(dt, TUP_FLAGS_CREATE) < 0)
			return -1;
		if(tup_db_set_dependent_dir_flags(dt, TUP_FLAGS_CREATE) < 0)
			return -1;
	}

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
	const char *file = NULL;
	tupid_t dt;

	len = canonicalize(path, cname, sizeof(cname), NULL);
	if(len < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", path);
		return -1;
	}

	dt = __create_dir_tupid(cname, 0, &file);
	if(dt < 0)
		return -1;

	tup_file_mod(dt, file, flags);

	return 0;
}

tupid_t find_dir_tupid(const char *dir)
{
	return __find_dir_tupid(dir, NULL);
}

tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last)
{
	char *slash;

	while(strncmp(dir, "../", 3) == 0) {
		dir += 3;
		dt = tup_db_parent(dt);
		if(dt < 0)
			return -1;
	}

	while((slash = strchr(dir, '/')) != NULL) {
		dt = tup_db_select_node_part(dt, dir, slash - dir);
		if(dt < 0)
			return -1;
		dir = slash + 1;
	}
	if(last) {
		*last = dir;
	} else {
		dt = tup_db_select_node(dt, dir);
		if(dt < 0)
			return -1;
	}
	return dt;
}

static tupid_t __find_dir_tupid(const char *dir, const char **last)
{
	tupid_t dotdt;

	dotdt = tup_db_select_node(0, ".");
	if(dotdt < 0)
		return -1;
	if(strcmp(dir, ".") == 0) {
		/* If the directory is the top, and we wanted to include the
		 * whole thing, then return that directory. If we want the
		 * parent of ".", then that's just zero.
		 */
		if(last) {
			*last = NULL;
			return 0;
		}
		return dotdt;
	}

	return find_dir_tupid_dt(dotdt, dir, last);
}

static tupid_t __create_dir_tupid(const char *dir, int include_last,
				  const char **last_part)
{
	char *slash;
	tupid_t dt;

	dt = tup_db_create_node(0, ".", TUP_NODE_DIR, TUP_FLAGS_CREATE);
	if(dt < 0)
		return -1;
	if(strcmp(dir, ".") == 0)
		return dt;

	while((slash = strchr(dir, '/')) != NULL) {
		dt = tup_db_create_node_part(dt, dir, slash - dir,
					     TUP_NODE_DIR, TUP_FLAGS_CREATE,
					     NULL);
		if(dt < 0)
			return -1;
		dir = slash + 1;
	}
	if(include_last) {
		dt = tup_db_create_node(dt,dir, TUP_NODE_DIR, TUP_FLAGS_CREATE);
		if(dt < 0)
			return -1;
	} else {
		if(last_part)
			*last_part = dir;
	}
	return dt;
}
