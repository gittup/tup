/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* _ATFILE_SOURCE for readlinkat */
#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "compat.h"
#include "pel_group.h"
#include "config.h"
#include "entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct id_flags {
	tupid_t tupid;
	int flags;
};

static int tup_del_id_type(tupid_t tupid, int type, int force);
static int ghost_to_file(struct tup_entry *tent);

static void (*rmdir_callback)(tupid_t tupid);

int create_name_file(tupid_t dt, const char *file, time_t mtime,
		     struct tup_entry **entry)
{
	if(tup_db_node_insert_tent(dt, file, -1, TUP_NODE_FILE, mtime, entry) < 0)
		return -1;
	if(tup_db_add_create_list(dt) < 0)
		return -1;
	return 0;
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	struct tup_entry *tent;
	tent = tup_db_create_node(dt, cmd, TUP_NODE_CMD);
	if(tent)
		return tent->tnode.tupid;
	return -1;
}

tupid_t create_dir_file(tupid_t dt, const char *path)
{
	struct tup_entry *tent;
	tent = tup_db_create_node(dt, path, TUP_NODE_DIR);
	if(tent)
		return tent->tnode.tupid;
	return -1;
}

tupid_t tup_file_mod(tupid_t dt, const char *file)
{
	int fd;
	struct stat buf;

	fd = tup_db_open_tupid(dt);
	if(fd < 0)
		return -1;
	if(fstatat(fd, file, &buf, AT_SYMLINK_NOFOLLOW) != 0) {
		fprintf(stderr, "tup error: tup_file_mod() fstatat failed.\n");
		perror(file);
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	return tup_file_mod_mtime(dt, file, buf.MTIME, 1);
}

tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force)
{
	struct tup_entry *tent;
	int new = 0;
	int modified = 0;

	if(tup_db_select_tent(dt, file, &tent) < 0)
		return -1;

	if(!tent) {
		if(create_name_file(dt, file, mtime, &tent) < 0)
			return -1;
		new = 1;
	} else {
		if(tent->mtime != mtime || force)
			modified = 1;

		if(tent->type == TUP_NODE_GHOST) {
			if(ghost_to_file(tent) < 0)
				return -1;
		} else if(tent->type != TUP_NODE_FILE &&
			  tent->type != TUP_NODE_GENERATED) {
			if(tup_del_id_type(tent->tnode.tupid, tent->type, 1) < 0)
				return -1;
			if(create_name_file(dt, file, mtime, &tent) < 0)
				return -1;
			new = 1;
		}
		if(modified) {
			if(tent->type == TUP_NODE_GENERATED) {
				int tmp = 0;
				if(tup_db_modify_cmds_by_output(tent->tnode.tupid, &tmp) < 0)
					return -1;
				if(tmp == 1) {
					fprintf(stderr, "tup warning: generated file '");
					print_tup_entry(stderr, tent);
					fprintf(stderr, "' was modified outside of tup. This file will be overwritten on the next update, unless the rule that creates it is also removed.\n");
				}
			}
			if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
				return -1;

			/* It's possible this is a file that was included by a
			 * Tupfile.  Try to set any dependent directory flags.
			 */
			if(tup_db_set_dependent_dir_flags(tent->tnode.tupid) < 0)
				return -1;

			if(tent->mtime != mtime)
				if(tup_db_set_mtime(tent, mtime) < 0)
					return -1;
		}
	}

	if(new || modified) {
		if(dt == DOT_DT && strcmp(file, TUP_CONFIG) == 0) {
			/* If tup.config was modified, put the @-directory in
			 * the create list so we can import any variables that
			 * have changed.
			 */
			if(tup_db_add_create_list(VAR_DT) < 0)
				return -1;
		}
	}

	return tent->tnode.tupid;
}

static int check_rm_tup_config(struct tup_entry *tent)
{
	if(tent->dt == DOT_DT && strcmp(tent->name.s, TUP_CONFIG) == 0) {
		/* If tup.config was removed, also add the @-directory to the
		 * create list.
		 */
		if(tup_db_add_create_list(VAR_DT) < 0)
			return -1;
	}
	return 0;
}

int tup_file_del(tupid_t dt, const char *file, int len)
{
	struct tup_entry *tent;

	if(len < 0)
		len = strlen(file);

	if(tup_db_select_tent_part(dt, file, len, &tent) < 0)
		return -1;
	if(!tent) {
		/* If we are trying to delete a file that isn't in tup, that's
		 * probably ok. This can happen if we create and delete a file
		 * real quick before the monitor can create the tup entry
		 * (t7037).
		 */
		return 0;
	}
	if(check_rm_tup_config(tent) < 0)
		return -1;

	/* If .gitignore is removed, make sure we re-parse the Tupfile
	 * (t7040).
	 */
	if(strncmp(file, ".gitignore", len) == 0 && len == 10) {
		if(tup_db_add_create_list(dt) < 0)
			return -1;
	}
	return tup_del_id_type(tent->tnode.tupid, tent->type, 0);
}

int tup_file_missing(struct tup_entry *tent)
{
	if(check_rm_tup_config(tent) < 0)
		return -1;
	return tup_del_id_type(tent->tnode.tupid, tent->type, 0);
}

int tup_del_id_force(tupid_t tupid, int type)
{
	return tup_del_id_type(tupid, type, 1);
}

void tup_register_rmdir_callback(void (*callback)(tupid_t tupid))
{
	rmdir_callback = callback;
}

static int tup_del_id_type(tupid_t tupid, int type, int force)
{
	if(type == TUP_NODE_DIR) {
		/* Recurse and kill anything below this dir. Note that
		 * tup_db_delete_dir() calls back to this function.
		 */
		if(tup_db_delete_dir(tupid) < 0)
			return -1;
		if(rmdir_callback)
			rmdir_callback(tupid);
	}

	/* If a file was deleted and it was created by a command, set the
	 * command's flags to modify. For example, if foo.o was deleted, we set
	 * 'gcc -c foo.c -o foo.o' to modify, so it will be re-executed. This
	 * only happens if a file was deleted outside of the parser (!force).
	 */
	if(type == TUP_NODE_GENERATED && !force) {
		int modified = 0;

		if(tup_db_modify_cmds_by_output(tupid, &modified) < 0)
			return -1;

		/* Since the file has been removed, make sure it is no longer
		 * in the modify list (t5071)
		 */
		if(tup_db_unflag_modify(tupid) < 0)
			return -1;

		/* Only display a warning if the command isn't already in the
		 * modify list. It's possible that the command hasn't actually
		 * been executed yet.
		 */
		if(modified == 1) {
			struct tup_entry *tent;

			tent = tup_entry_find(tupid);
			if(!tent) {
				fprintf(stderr, "tup warning: generated file ID %lli was deleted outside of tup. This file may be re-created on the next update.\n", tupid);
			} else {
				fprintf(stderr, "tup warning: generated file '");
				print_tup_entry(stderr, tent);
				fprintf(stderr, "' was deleted outside of tup. This file may be re-created on the next update.\n");
			}
		}

		/* If we're not forcing the deletion, just return here (the
		 * node won't actually be removed from tup). The fact that the
		 * command is in modify will take care of dependencies, and
		 * we don't want to put the directory back in create (t6036).
		 */
		return 0;
	}

	if(type == TUP_NODE_FILE || type == TUP_NODE_DIR) {
		/* It's possible this is a file that was included by a Tupfile.
		 * Try to set any dependent directory flags.
		 */
		if(tup_db_set_dependent_dir_flags(tupid) < 0)
			return -1;
	}

	if(type == TUP_NODE_FILE || type == TUP_NODE_GENERATED) {
		/* We also have to run any command that used this file as an
		 * input, so we can yell at the user if they haven't already
		 * fixed that command.
		 */
		if(tup_db_modify_cmds_by_input(tupid) < 0)
			return -1;

		if(!force) {
			/* Re-parse the current Tupfile (the updater
			 * automatically parses any dependent directories).
			 */
			if(tup_db_add_dir_create_list(tupid) < 0)
				return -1;
		}
	}
	if(delete_name_file(tupid) < 0)
		return -1;
	return 0;
}

struct tup_entry *get_tent_dt(tupid_t dt, const char *path)
{
	struct path_element *pel = NULL;
	struct tup_entry *tent;

	dt = find_dir_tupid_dt(dt, path, &pel, 0);
	if(dt < 0)
		return NULL;

	if(pel) {
		if(tup_db_select_tent_part(dt, pel->path, pel->len, &tent) < 0)
			return NULL;
		free(pel);
		if(!tent)
			return NULL;
		return tent;
	} else {
		/* We get here if the path list ends up being empty (for
		 * example, if the path is ".")
		 */
		return tup_entry_get(dt);
	}
}

tupid_t find_dir_tupid(const char *dir)
{
	struct tup_entry *tent;

	/* This check is used for tests to get the parent tupid for the '.'
	 * directory.
	 */
	if(strcmp(dir, "0") == 0)
		return 0;
	tent = get_tent_dt(DOT_DT, dir);
	if(!tent)
		return -1;
	return tent->tnode.tupid;
}

tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, int sotgv)
{
	struct pel_group pg;
	tupid_t tupid;

	if(get_path_elements(dir, &pg) < 0)
		return -1;

	tupid = find_dir_tupid_dt_pg(dt, &pg, last, sotgv);
	return tupid;
}

tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last, int sotgv)
{
	struct path_element *pel;
	struct tup_entry *tent;

	/* Ignore if the file is hidden or outside of the tup hierarchy */
	if((pg->pg_flags & PG_HIDDEN) || (pg->pg_flags & PG_OUTSIDE_TUP))
		return 0;

	/* The list can be empty if dir is "." or something like "foo/..". In
	 * this case just return dt (the start dir).
	 */
	if(TAILQ_EMPTY(&pg->path_list)) {
		if(tup_entry_add(dt, &tent) < 0)
			return -1;
		return dt;
	}

	if(last) {
		pel = TAILQ_LAST(&pg->path_list, path_element_head);
		*last = pel;
		TAILQ_REMOVE(&pg->path_list, pel, list);
	} else {
		/* TODO */
		fprintf(stderr, "[31mBork[0m\n");
		exit(1);
	}

	if(pg->pg_flags & PG_ROOT)
		dt = 1;
	if(tup_entry_add(dt, &tent) < 0)
		return -1;

	while(!TAILQ_EMPTY(&pg->path_list)) {
		pel = TAILQ_FIRST(&pg->path_list);
		if(pel->len == 2 && pel->path[0] == '.' && pel->path[1] == '.') {
			if(tent->parent == NULL) {
				/* If we're at the top of the tup hierarchy and
				 * trying to go up a level, bail out and return
				 * success since we don't keep track of files
				 * in the great beyond.
				 */
				free(*last);
				*last = NULL;
				del_pel_group(pg);
				return 0;
			}
			tent = tent->parent;
		} else {
			tupid_t curdt;

			curdt = tent->tnode.tupid;
			if(tup_db_select_tent_part(curdt, pel->path, pel->len, &tent) < 0)
				return -1;
			if(!tent) {
				/* Secret of the ghost valley! */
				if(sotgv == 0) {
					fprintf(stderr, "tup error: Expected node '%.*s' to be in directory %lli, but it is not there.\n", pel->len, pel->path, curdt);
					return -1;
				}
				if(tup_db_node_insert_tent(curdt, pel->path, pel->len, TUP_NODE_GHOST, -1, &tent) < 0)
					return -1;
			}
		}

		del_pel(pel, pg);
	}

	return tent->tnode.tupid;
}

int gimme_tent(const char *name, struct tup_entry **entry)
{
	tupid_t dt;
	struct path_element *pel = NULL;

	dt = find_dir_tupid_dt(DOT_DT, name, &pel, 0);
	if(dt < 0)
		return -1;
	if(dt == 0) {
		*entry = NULL;
		return 0;
	}
	if(pel == NULL) {
		*entry = tup_entry_get(dt);
		return 0;
	}
	if(tup_db_select_tent_part(dt, pel->path, pel->len, entry) < 0)
		return -1;
	free(pel);
	return 0;
}

int gimme_tent_or_make_ghost(tupid_t dt, const char *name,
			     struct tup_entry **entry)
{
	tupid_t new_dt;
	struct path_element *pel = NULL;

	new_dt = find_dir_tupid_dt(dt, name, &pel, 1);
	if(new_dt < 0)
		return -1;

	if(new_dt == 0) {
		*entry = NULL;
		return 0;
	}
	if(pel == NULL) {
		*entry = tup_entry_get(new_dt);
		return 0;
	}

	if(tup_db_select_tent_part(new_dt, pel->path, pel->len, entry) < 0)
		return -1;
	if(!*entry) {
		if(tup_db_node_insert_tent(new_dt, pel->path, pel->len, TUP_NODE_GHOST, -1, entry) < 0) {
			fprintf(stderr, "Error: Node '%.*s' doesn't exist in directory %lli, and no luck creating a ghost node there.\n", pel->len, pel->path, new_dt);
			return -1;
		}
	}
	free(pel);

	return 0;
}

static int ghost_to_file(struct tup_entry *tent)
{
	if(tup_db_set_type(tent, TUP_NODE_FILE) < 0)
		return -1;
	if(tup_db_add_create_list(tent->dt) < 0)
		return -1;
	if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
		return -1;
	return 0;
}
