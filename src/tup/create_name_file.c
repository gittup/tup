#include "fileio.h"
#include "db.h"
#include "compat.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct id_flags {
	tupid_t tupid;
	int flags;
};

static tupid_t __find_dir_tupid(const char *dir, const char **last,
				struct list_head *symlist);

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

tupid_t update_symlink_file(tupid_t dt, const char *file)
{
	int rc;
	tupid_t tupid;
	tupid_t link_dt;
	tupid_t link_tupid;
	static char linkname[PATH_MAX];

	tupid = tup_db_select_node(dt, file);
	if(tupid < 0) {
		tupid = create_name_file(dt, file);
		if(tupid < 0)
			return -1;
	}

	rc = readlink(file, linkname, sizeof(linkname));
	if(rc < 0) {
		fprintf(stderr, "readlink: ");
		perror(file);
		return -1;
	}
	if(rc >= (signed)sizeof(linkname)) {
		fprintf(stderr, "tup error: linkname buffer is too small for the symlink of '%s'\n", file);
		return -1;
	}
	linkname[rc] = 0;

	/* TODO: What if the symlink is a full path? Shouldn't
	 * be relative to dt then.
	 */
	link_dt = find_dir_tupid_dt(dt, linkname, &file, NULL);
	if(link_dt < 0)
		return -1;
	link_tupid = tup_db_select_node(link_dt, file);
	if(link_tupid < 0) {
		fprintf(stderr, "Error: Unable to find node '%s' in directory %lli in order to symlink %s\n", file, link_dt, file);
		return -1;
	}
	if(tup_db_set_sym(tupid, link_tupid) < 0)
		return -1;
	if(tup_db_add_modify_list(tupid) < 0)
		return -1;
	return tupid;
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

tupid_t tup_file_mod(tupid_t dt, const char *file, int flags)
{
	struct db_node dbn;

	if(tup_db_select_dbn(dt, file, &dbn) < 0)
		return -1;

	if(flags == TUP_FLAGS_MODIFY) {
		/* Need to re-parse the Tupfile if file is new to the database,
		 * or if the file itself is the Tupfile.
		 */
		if(dbn.tupid < 0 || strcmp(file, "Tupfile") == 0) {
			if(tup_db_add_create_list(dt) < 0)
				return -1;
		}

		if(dbn.tupid < 0) {
			dbn.tupid = create_name_file(dt, file);
			if(dbn.tupid < 0)
				return -1;
		} else {
			if(dbn.type != TUP_NODE_FILE &&
			   dbn.type != TUP_NODE_GENERATED) {
				fprintf(stderr, "tup error: tup_file_mod() expecting to move a file to the modify_list, but got type: %i\n", dbn.type);
				return -1;
			}
			if(tup_db_set_flags_by_id(dbn.tupid, flags) < 0)
				return -1;

			/* It's possible this is a file that was included by a
			 * Tupfile.  Try to set any dependent directory flags.
			 */
			if(tup_db_set_dependent_dir_flags(dbn.tupid) < 0)
				return -1;
		}
		return dbn.tupid;
	} else if(flags == TUP_FLAGS_DELETE) {
		if(dbn.tupid < 0) {
			fprintf(stderr, "[31mError: Trying to delete file '%s', which isn't in .tup/db[0m\n", file);
			return -1;
		}
		return tup_file_del(dbn.tupid, dbn.dt, dbn.type);
	} else {
		fprintf(stderr, "tup error: Unknown flags argument to tup_file_mod(): %i\n", flags);
		return -1;
	}
}

int tup_file_del(tupid_t tupid, tupid_t dt, int type)
{
	if(type == TUP_NODE_DIR) {
		/* Directories are pretty simple, but we need to recurse and
		 * kill anything underneath the diretory as well.
		 */
		if(tup_db_delete_dir(tupid) < 0)
			return -1;
		if(delete_name_file(tupid) < 0)
			return -1;
		return 0;
	}
	/* If a file was deleted and it was created by a command, set the
	 * command's flags to modify. For example, if foo.o was deleted, we set
	 * 'gcc -c foo.c -o foo.o' to modify, so it will be re-executed.
	 *
	 * This is really just to mimic what people would expect from make.
	 * Randomly deleting object files is pretty stupid.
	 */
	if(type == TUP_NODE_GENERATED)
		if(tup_db_modify_cmds_by_output(tupid) < 0)
			return -1;

	/* We also have to run any command that used this file as an input, so
	 * we can yell at the user if they haven't already fixed that command.
	 */
	if(tup_db_modify_cmds_by_input(tupid) < 0)
		return -1;

	/* Re-parse the current Tupfile (the updater automatically parses any
	 * dependent directories).
	 */
	if(tup_db_add_create_list(dt) < 0)
		return -1;

	/* It's possible this is a file that was included by a Tupfile.  Try to
	 * set any dependent directory flags.
	 */
	if(tup_db_set_dependent_dir_flags(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(delete_name_file(tupid) < 0)
		return -1;
	return 0;
}

tupid_t tup_pathname_mod(const char *path, int flags)
{
	static char cname[PATH_MAX];
	int len;
	const char *file = NULL;
	tupid_t dt;

	len = canonicalize(path, cname, sizeof(cname), NULL, get_sub_dir());
	if(len < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", path);
		return -1;
	}

	dt = __find_dir_tupid(cname, &file, NULL);
	if(dt < 0) {
		fprintf(stderr, "Unable to find directory for '%s'\n", cname);
		return -1;
	}

	return tup_file_mod(dt, file, flags);
}

tupid_t get_dbn_dt(tupid_t dt, const char *path, struct db_node *dbn,
		   struct list_head *symlist)
{
	const char *file;

	dt = find_dir_tupid_dt(dt, path, &file, symlist);
	if(dt < 0)
		return -1;

	if(file) {
		if(tup_db_select_dbn(dt, file, dbn) < 0)
			return -1;
		while(dbn->sym != -1) {
			if(symlist) {
				struct half_entry *he;
				he = malloc(sizeof *he);
				if(!he) {
					perror("malloc");
					return -1;
				}
				he->tupid = dbn->tupid;
				he->dt = dbn->dt;
				he->type = dbn->type;
				list_add(&he->list, symlist);
			}

			if(tup_db_select_dbn_by_id(dbn->sym, dbn) < 0)
				return -1;
		}
		return dbn->tupid;
	} else {
		fprintf(stderr, "tup TODO: no dbn?\n");
		return -1;
		return dt;
	}
}

tupid_t get_dbn(const char *path, struct db_node *dbn)
{
	return get_dbn_and_sym_tupids(path, dbn, NULL);
}

tupid_t get_dbn_and_sym_tupids(const char *path, struct db_node *dbn,
			       struct list_head *symlist)
{
	const char *file;
	tupid_t dt;

	dt = __find_dir_tupid(path, &file, symlist);
	if(dt < 0)
		return -1;

	if(file) {
		if(tup_db_select_dbn(dt, file, dbn) < 0)
			return -1;
		while(dbn->sym != -1) {
			if(symlist) {
				struct half_entry *he;
				he = malloc(sizeof *he);
				if(!he) {
					perror("malloc");
					return -1;
				}
				he->tupid = dbn->tupid;
				he->dt = dbn->dt;
				he->type = dbn->type;
				list_add(&he->list, symlist);
			}

			if(tup_db_select_dbn_by_id(dbn->sym, dbn) < 0)
				return -1;
		}
		return dbn->tupid;
	} else {
		fprintf(stderr, "tup TODO: no dbn?\n");
		return -1;
		return dt;
	}
}

tupid_t find_dir_tupid(const char *dir)
{
	return __find_dir_tupid(dir, NULL, NULL);
}

tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last,
			  struct list_head *symlist)
{
	char *slash;

	while(strncmp(dir, "../", 3) == 0) {
		dir += 3;
		dt = tup_db_parent(dt);
		if(dt < 0)
			return -1;
	}

	while((slash = strchr(dir, '/')) != NULL) {
		struct db_node dbn;

		if(slash-dir == 1 && dir[0] == '.')
			goto next_dir;
		if(tup_db_select_dbn_part(dt, dir, slash - dir, &dbn) < 0)
			return -1;

		while(dbn.sym != -1) {
			if(symlist) {
				struct half_entry *he;
				he = malloc(sizeof *he);
				if(!he) {
					perror("malloc");
					return -1;
				}
				he->tupid = dbn.tupid;
				he->dt = dbn.dt;
				he->type = dbn.type;
				list_add(&he->list, symlist);
			}

			if(tup_db_select_dbn_by_id(dbn.sym, &dbn) < 0)
				return -1;
		}
		dt = dbn.tupid;
next_dir:
		dir = slash + 1;
	}
	if(last) {
		*last = dir;
	} else {
		struct db_node dbn;

		if(tup_db_select_dbn(dt, dir, &dbn) < 0)
			return -1;
		while(dbn.sym != -1) {
			if(symlist) {
				struct half_entry *he;
				he = malloc(sizeof *he);
				if(!he) {
					perror("malloc");
					return -1;
				}
				he->tupid = dbn.tupid;
				he->dt = dbn.dt;
				he->type = dbn.type;
				list_add(&he->list, symlist);
			}

			if(tup_db_select_dbn_by_id(dbn.sym, &dbn) < 0)
				return -1;
		}
		dt = dbn.tupid;
	}
	return dt;
}

static tupid_t __find_dir_tupid(const char *dir, const char **last,
				struct list_head *symlist)
{
	if(strcmp(dir, ".") == 0) {
		/* If the directory is the top, and we wanted to include the
		 * whole thing, then return that directory. If we want the
		 * parent of ".", then that's just zero.
		 */
		if(last) {
			*last = dir;
			return 0;
		}
		return DOT_DT;
	}

	return find_dir_tupid_dt(DOT_DT, dir, last, symlist);
}
