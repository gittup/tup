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

tupid_t create_name_file(tupid_t dt, const char *file)
{
	return tup_db_create_node(dt, file, TUP_NODE_FILE);
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	return tup_db_create_node(dt, cmd, TUP_NODE_CMD);
}

tupid_t create_dir_file(tupid_t dt, const char *path)
{
	return tup_db_create_node(dt, path, TUP_NODE_DIR);
}

tupid_t create_var_file(const char *var, const char *value)
{
	int rc;
	struct db_node dbn;

	if(tup_db_select_dbn(VAR_DT, var, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0) {
		dbn.tupid = tup_db_create_node(VAR_DT, var, TUP_NODE_VAR);
		if(dbn.tupid < 0)
			return -1;
	} else {
		char *orig_value;
		if(tup_db_get_var_id(dbn.tupid, &orig_value) < 0)
			return -1;
		rc = strcmp(orig_value, value);
		free(orig_value);
		/* If the value hasn't changed, just clear the flags */
		if(rc == 0) {
			if(tup_db_unflag_delete(dbn.tupid) < 0)
				return -1;
			return 0;
		}

		if(tup_db_add_create_list(dbn.tupid) < 0)
			return -1;
		if(tup_db_add_modify_list(dbn.tupid) < 0)
			return -1;
		if(tup_db_unflag_delete(dbn.tupid) < 0)
			return -1;
	}
	return tup_db_set_var(dbn.tupid, value);
}

int tup_file_mod(tupid_t dt, const char *file, int flags)
{
	int upddir = 0;
	struct db_node dbn;

	/* Tried to simplify the gross if logic. Basically we want to re-update
	 * the create nodes if:
	 * 1) the file is new
	 * 2) the file was deleted
	 * 3) a Tupfile was modified
	 */
	if(tup_db_select_dbn(dt, file, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0)
		upddir = 1;
	if(flags == TUP_FLAGS_DELETE)
		upddir = 1;
	if(strcmp(file, "Tupfile") == 0)
		upddir = 1;

	if(upddir) {
		if(tup_db_add_create_list(dt) < 0)
			return -1;
	}

	if(dbn.tupid < 0) {
		if(flags == TUP_FLAGS_DELETE) {
			fprintf(stderr, "[31mError: Trying to delete file '%s', which isn't in .tup/db[0m\n", file);
			return -1;
		}
		dbn.tupid = create_name_file(dt, file);
		if(dbn.tupid < 0)
			return -1;
	} else {
		/* Directories that are deleted get special treatment, since we
		 * recurse and delete all sub-nodes.
		 */
		if(flags == TUP_FLAGS_DELETE && dbn.type == TUP_NODE_DIR) {
			if(tup_db_delete_dir(dbn.tupid) < 0)
				return -1;
		} else {
			/* If a file was deleted and it was created by a
			 * command, set the command's flags to modify. For
			 * example, if foo.o was deleted, we set 'gcc -c foo.c
			 * -o foo.o' to modify, so it will be re-executed.
			 *
			 * This is really just to mimic what people would
			 * expect from make.  Randomly deleting object files is
			 * pretty stupid.
			 */
			if(flags == TUP_FLAGS_DELETE) {
				if(tup_db_set_cmd_flags_by_output(dbn.tupid, TUP_FLAGS_MODIFY) < 0)
					return -1;
			}
			if(tup_db_set_flags_by_id(dbn.tupid, flags) < 0)
				return -1;
		}
	}

	/* It's possible this is a file that was included by a Tupfile. Try to
	 * set any dependent directory flags.
	 */
	if(dbn.type == TUP_NODE_FILE) {
		if(tup_db_set_dependent_dir_flags(dbn.tupid) < 0)
			return -1;
	}

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

	if(tup_file_mod(dt, file, flags) < 0)
		return -1;

	return 0;
}

tupid_t get_dbn(const char *path, struct db_node *dbn)
{
	const char *file;
	tupid_t dt;

	if(strcmp(path, ".") == 0) {
		if(tup_db_select_dbn(0, path, dbn) < 0)
			return -1;
		return dbn->tupid;
	}

	dt = __find_dir_tupid(path, &file);
	if(dt < 0)
		return -1;

	if(file) {
		if(tup_db_select_dbn(dt, file, dbn) < 0)
			return -1;
		return dbn->tupid;
	} else {
		printf("TODO: no dbn?\n");
		return -1;
		return dt;
	}
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
		if(slash-dir == 1 && dir[0] == '.')
			goto next_dir;
		dt = tup_db_select_node_part(dt, dir, slash - dir);
		if(dt < 0)
			return -1;
next_dir:
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
	if(strcmp(dir, ".") == 0) {
		/* If the directory is the top, and we wanted to include the
		 * whole thing, then return that directory. If we want the
		 * parent of ".", then that's just zero.
		 */
		if(last) {
			*last = NULL;
			return 0;
		}
		return DOT_DT;
	}

	return find_dir_tupid_dt(DOT_DT, dir, last);
}

static tupid_t __create_dir_tupid(const char *dir, int include_last,
				  const char **last_part)
{
	char *slash;
	tupid_t dt;

	dt = tup_db_create_node(0, ".", TUP_NODE_DIR);
	if(dt < 0)
		return -1;
	if(strcmp(dir, ".") == 0) {
		if(include_last) {
			*last_part = NULL;
			return dt;
		} else {
			*last_part = ".";
			return 0;
		}
	}

	while((slash = strchr(dir, '/')) != NULL) {
		dt = tup_db_create_node_part(dt, dir, slash - dir, TUP_NODE_DIR);
		if(dt < 0)
			return -1;
		dir = slash + 1;
	}
	if(include_last) {
		dt = tup_db_create_node(dt,dir, TUP_NODE_DIR);
		if(dt < 0)
			return -1;
	} else {
		if(last_part)
			*last_part = dir;
	}
	return dt;
}
