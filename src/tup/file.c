/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2013  Mike Shal <marfey@gmail.com>
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

#define _ATFILE_SOURCE
#include "file.h"
#include "debug.h"
#include "db.h"
#include "fileio.h"
#include "config.h"
#include "entry.h"
#include "option.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

static struct file_entry *new_entry(const char *filename);
static void check_unlink_list(const struct pel_group *pg,
			      struct file_entry_head *u_head);
static void handle_unlink(struct file_info *info);
static int update_write_info(FILE *f, tupid_t cmdid, struct file_info *info,
			     int *warnings, struct tup_entry_head *entryhead);
static int update_read_info(FILE *f, tupid_t cmdid, struct file_info *info,
			    struct tup_entry_head *entryhead,
			    struct tupid_entries *sticky_root,
			    struct tupid_entries *normal_root,
			    struct tupid_entries *group_sticky_root,
			    int full_deps, tupid_t vardt,
			    struct tupid_entries *used_groups_root,
			    int *important_link_removed);
static int add_config_files_locked(struct file_info *finfo, struct tup_entry *tent);
static int add_parser_files_locked(FILE *f, struct file_info *finfo,
				   struct tupid_entries *root, tupid_t vardt);

int init_file_info(struct file_info *info, const char *variant_dir)
{
	LIST_INIT(&info->read_list);
	LIST_INIT(&info->write_list);
	LIST_INIT(&info->unlink_list);
	LIST_INIT(&info->var_list);
	LIST_INIT(&info->mapping_list);
	LIST_INIT(&info->tmpdir_list);
	pthread_mutex_init(&info->lock, NULL);
	/* Root variant gets a NULL variant_dir so we can skip trying to do the
	 * same thing twice in the server (eg: we only need a single readdir()
	 * on the src tree).
	 */
	if(variant_dir[0])
		info->variant_dir = variant_dir;
	else
		info->variant_dir = NULL;
	info->server_fail = 0;
	info->open_count = 0;
	return 0;
}

void finfo_lock(struct file_info *info)
{
	pthread_mutex_lock(&info->lock);
}

void finfo_unlock(struct file_info *info)
{
	pthread_mutex_unlock(&info->lock);
}

int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info)
{
	DEBUGP("received file '%s' in mode %i\n", filename, at);
	int rc;

	finfo_lock(info);
	if(at == ACCESS_RENAME) {
		rc = handle_rename(filename, file2, info);
	} else {
		rc = handle_open_file(at, filename, info);
	}
	finfo_unlock(info);

	return rc;
}

int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info)
{
	struct file_entry *fent;
	int rc = 0;

	fent = new_entry(filename);
	if(!fent) {
		return -1;
	}

	switch(at) {
		case ACCESS_READ:
			LIST_INSERT_HEAD(&info->read_list, fent, list);
			break;
		case ACCESS_WRITE:
			check_unlink_list(&fent->pg, &info->unlink_list);
			LIST_INSERT_HEAD(&info->write_list, fent, list);
			break;
		case ACCESS_UNLINK:
			LIST_INSERT_HEAD(&info->unlink_list, fent, list);
			break;
		case ACCESS_VAR:
			LIST_INSERT_HEAD(&info->var_list, fent, list);
			break;
		case ACCESS_RENAME:
		default:
			fprintf(stderr, "Invalid event type: %i\n", at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(FILE *f, tupid_t cmdid, struct file_info *info, int *warnings,
		int check_only, struct tupid_entries *sticky_root,
		struct tupid_entries *normal_root,
		struct tupid_entries *group_sticky_root,
		int full_deps, tupid_t vardt,
		struct tupid_entries *used_groups_root,
		int *important_link_removed)
{
	struct tup_entry_head *entrylist;
	struct tmpdir *tmpdir;
	int tmpdir_bork = 0;
	int rc1 = 0, rc2;

	finfo_lock(info);
	handle_unlink(info);

	if(!check_only) {
		LIST_FOREACH(tmpdir, &info->tmpdir_list, list) {
			fprintf(f, "tup error: Directory '%s' was created, but not subsequently removed. Only temporary directories can be created by commands.\n", tmpdir->dirname);
			tmpdir_bork = 1;
		}
		if(tmpdir_bork) {
			finfo_unlock(info);
			return -1;
		}
	}

	entrylist = tup_entry_get_list();
	rc1 = update_write_info(f, cmdid, info, warnings, entrylist);
	tup_entry_release_list();

	entrylist = tup_entry_get_list();
	rc2 = update_read_info(f, cmdid, info, entrylist, sticky_root, normal_root, group_sticky_root, full_deps, vardt, used_groups_root, important_link_removed);
	tup_entry_release_list();
	finfo_unlock(info);

	if(rc1 == 0 && rc2 == 0)
		return 0;
	return -1;
}

int add_config_files(struct file_info *finfo, struct tup_entry *tent)
{
	int rc;
	finfo_lock(finfo);
	rc = add_config_files_locked(finfo, tent);
	finfo_unlock(finfo);
	return rc;
}

int add_parser_files(FILE *f, struct file_info *finfo, struct tupid_entries *root, tupid_t vardt)
{
	int rc;
	finfo_lock(finfo);
	rc = add_parser_files_locked(f, finfo, root, vardt);
	finfo_unlock(finfo);
	return rc;
}

/* Ghost directories in the /-tree have mtimes set to zero if they exist. This way we can
 * distinguish between a directory being created where there wasn't one previously (t4064).
 */
static int set_directories_to_zero(tupid_t dt, tupid_t slash)
{
	struct tup_entry *tent;

	if(dt == slash)
		return 0;
	if(tup_entry_add(dt, &tent) < 0)
		return -1;

	/* Short circuit if we found a dir that is already set */
	if(tent->mtime == 0)
		return 0;

	if(tup_db_set_mtime(tent, 0) < 0)
		return -1;
	return set_directories_to_zero(tent->dt, slash);
}

static int add_node_to_list(FILE *f, tupid_t dt, struct pel_group *pg,
			    struct tup_entry_head *head, int full_deps, const char *full_path)
{
	tupid_t new_dt;
	struct path_element *pel = NULL;
	struct tup_entry *tent;

	new_dt = find_dir_tupid_dt_pg(f, dt, pg, &pel, 1, full_deps);
	if(new_dt < 0)
		return -1;
	if(new_dt == 0) {
		return 0;
	}
	if(pel == NULL) {
		/* This can happen for the '.' entry */
		return 0;
	}

	if(tup_db_select_tent_part(new_dt, pel->path, pel->len, &tent) < 0)
		return -1;
	if(!tent) {
		time_t mtime = -1;
		if(full_deps && (pg->pg_flags & PG_OUTSIDE_TUP)) {
			struct stat buf;
			if(lstat(full_path, &buf) == 0) {
				mtime = buf.MTIME;
			}
			if(set_directories_to_zero(new_dt, slash_dt()) < 0)
				return -1;
		}
		/* Note that full-path entries are always ghosts since we don't scan them. They
		 * can still have a valid mtime, though.
		 */
		if(tup_db_node_insert_tent(new_dt, pel->path, pel->len, TUP_NODE_GHOST, mtime, -1, &tent) < 0) {
			fprintf(stderr, "tup error: Node '%.*s' doesn't exist in directory %lli, and no luck creating a ghost node there.\n", pel->len, pel->path, new_dt);
			return -1;
		}
	}
	free(pel);

	if(tent->type == TUP_NODE_DIR || tent->type == TUP_NODE_GENERATED_DIR) {
		/* We don't track dependencies on directory nodes for commands. Note that
		 * some directory accesses may create ghost nodes as placeholders for the
		 * directory until a real directory is created there (eg: t5077). In this
		 * case, once the directory is created, we lose the dependency on the ghost
		 * dir and get the actual dependency on the file (or ghost file) in the
		 * directory instead.
		 */
		return 0;
	}
	tup_entry_list_add(tent, head);

	/* TODO: This seems to fix t2033 - is it acceptable? For some reason
	   Windows gives us events on secret and secret/ghost, but OSX only
	   has the event on secret/ghost.
	 */
	while(tent->type == TUP_NODE_GHOST) {
		tent = tent->parent;
		if(tent->type == TUP_NODE_DIR)
			break;
		tup_entry_list_add(tent, head);
	}

	return 0;
}

static int file_set_mtime(struct tup_entry *tent, const char *file)
{
	struct stat buf;
	if(fstatat(tup_top_fd(), file, &buf, AT_SYMLINK_NOFOLLOW) < 0) {
		fprintf(stderr, "tup error: file_set_mtime() fstatat failed.\n");
		perror(file);
		return -1;
	}
	if(S_ISFIFO(buf.st_mode)) {
		fprintf(stderr, "tup error: Unable to create a FIFO as an output file. They can only be used as temporary files.\n");
		return -1;
	}
	if(tup_db_set_mtime(tent, buf.MTIME) < 0)
		return -1;
	return 0;
}

static int add_config_files_locked(struct file_info *finfo, struct tup_entry *tent)
{
	struct file_entry *r;
	struct tup_entry_head *entrylist;
	int full_deps = tup_option_get_int("updater.full_deps");

	entrylist = tup_entry_get_list();
	while(!LIST_EMPTY(&finfo->read_list)) {
		struct tup_entry *tmp;
		r = LIST_FIRST(&finfo->read_list);

		if(add_node_to_list(stderr, DOT_DT, &r->pg, entrylist, full_deps, r->filename) < 0)
			return -1;

		/* Don't link to ourself */
		tmp = LIST_FIRST(entrylist);
		if(tmp == tent) {
			tup_entry_list_del(tmp);
		}

		del_file_entry(r);
	}
	if(tup_db_check_config_inputs(tent, entrylist) < 0)
		return -1;
	tup_entry_release_list();

	return 0;
}

static int add_parser_files_locked(FILE *f, struct file_info *finfo,
				   struct tupid_entries *root, tupid_t vardt)
{
	struct file_entry *r;
	struct mapping *map;
	struct tup_entry_head *entrylist;
	struct tup_entry *tent;
	int map_bork = 0;
	int full_deps = tup_option_get_int("updater.full_deps");

	entrylist = tup_entry_get_list();
	while(!LIST_EMPTY(&finfo->read_list)) {
		r = LIST_FIRST(&finfo->read_list);
		if(add_node_to_list(f, DOT_DT, &r->pg, entrylist, full_deps, r->filename) < 0)
			return -1;
		del_file_entry(r);
	}
	while(!LIST_EMPTY(&finfo->var_list)) {
		r = LIST_FIRST(&finfo->var_list);

		if(add_node_to_list(f, vardt, &r->pg, entrylist, 0, NULL) < 0)
			return -1;
		del_file_entry(r);
	}
	LIST_FOREACH(tent, entrylist, list) {
		if(strcmp(tent->name.s, ".gitignore") != 0)
			if(tupid_tree_add_dup(root, tent->tnode.tupid) < 0)
				return -1;
	}
	tup_entry_release_list();

	/* TODO: write_list not needed here? */
	while(!LIST_EMPTY(&finfo->write_list)) {
		r = LIST_FIRST(&finfo->write_list);
		del_file_entry(r);
	}

	while(!LIST_EMPTY(&finfo->mapping_list)) {
		map = LIST_FIRST(&finfo->mapping_list);

		if(gimme_tent(map->realname, &tent) < 0)
			return -1;
		if(!tent) {
			fprintf(stderr, "tup error: Writing to file '%s' while parsing is not allowed\n", map->realname);
			map_bork = 1;
		}
		del_map(map);
	}
	if(map_bork)
		return -1;
	return 0;
}

static struct file_entry *new_entry(const char *filename)
{
	struct file_entry *fent;

	fent = malloc(sizeof *fent);
	if(!fent) {
		perror("malloc");
		return NULL;
	}

	fent->filename = strdup(filename);
	if(!fent->filename) {
		perror("strdup");
		free(fent);
		return NULL;
	}

	if(get_path_elements(fent->filename, &fent->pg) < 0) {
		free(fent->filename);
		free(fent);
		return NULL;
	}
	return fent;
}

void del_file_entry(struct file_entry *fent)
{
	LIST_REMOVE(fent, list);
	del_pel_group(&fent->pg);
	free(fent->filename);
	free(fent);
}

int handle_rename(const char *from, const char *to, struct file_info *info)
{
	struct file_entry *fent;
	struct pel_group pg_from;
	struct pel_group pg_to;

	if(get_path_elements(from, &pg_from) < 0)
		return -1;
	if(get_path_elements(to, &pg_to) < 0)
		return -1;

	LIST_FOREACH(fent, &info->write_list, list) {
		if(pg_eq(&fent->pg, &pg_from)) {
			del_pel_group(&fent->pg);
			free(fent->filename);

			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
			if(get_path_elements(fent->filename, &fent->pg) < 0)
				return -1;
		}
	}
	LIST_FOREACH(fent, &info->read_list, list) {
		if(pg_eq(&fent->pg, &pg_from)) {
			del_pel_group(&fent->pg);
			free(fent->filename);

			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
			if(get_path_elements(fent->filename, &fent->pg) < 0)
				return -1;
		}
	}

	check_unlink_list(&pg_to, &info->unlink_list);
	del_pel_group(&pg_to);
	del_pel_group(&pg_from);
	return 0;
}

void del_map(struct mapping *map)
{
	LIST_REMOVE(map, list);
	free(map->tmpname);
	free(map->realname);
	free(map);
}

static void check_unlink_list(const struct pel_group *pg,
			      struct file_entry_head *u_head)
{
	struct file_entry *fent, *tmp;

	LIST_FOREACH_SAFE(fent, u_head, list, tmp) {
		if(pg_eq(&fent->pg, pg)) {
			del_file_entry(fent);
		}
	}
}

static void handle_unlink(struct file_info *info)
{
	struct file_entry *u, *fent, *tmp;

	while(!LIST_EMPTY(&info->unlink_list)) {
		u = LIST_FIRST(&info->unlink_list);

		LIST_FOREACH_SAFE(fent, &info->write_list, list, tmp) {
			if(pg_eq(&fent->pg, &u->pg)) {
				del_file_entry(fent);
			}
		}
		LIST_FOREACH_SAFE(fent, &info->read_list, list, tmp) {
			if(pg_eq(&fent->pg, &u->pg)) {
				del_file_entry(fent);
			}
		}

		del_file_entry(u);
	}
}

static int update_write_info(FILE *f, tupid_t cmdid, struct file_info *info,
			     int *warnings, struct tup_entry_head *entryhead)
{
	struct file_entry *w;
	struct file_entry *r;
	struct file_entry *tmp;
	struct tup_entry *tent;
	int write_bork = 0;

	while(!LIST_EMPTY(&info->write_list)) {
		tupid_t newdt;
		struct path_element *pel = NULL;

		w = LIST_FIRST(&info->write_list);

		/* Remove duplicate write entries */
		LIST_FOREACH_SAFE(r, &info->write_list, list, tmp) {
			if(r != w && pg_eq(&w->pg, &r->pg)) {
				del_file_entry(r);
			}
		}

		if(w->pg.pg_flags & PG_HIDDEN) {
			if(warnings) {
				fprintf(f, "tup warning: Writing to hidden file '%s'\n", w->filename);
				(*warnings)++;
			}
			goto out_skip;
		}

		newdt = find_dir_tupid_dt_pg(f, DOT_DT, &w->pg, &pel, 0, 0);
		if(newdt <= 0) {
			fprintf(f, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output\n", w->filename);
			return -1;
		}
		if(!pel) {
			fprintf(f, "[31mtup internal error: find_dir_tupid_dt_pg() in write_files() didn't get a final pel pointer.[0m\n");
			return -1;
		}

		if(tup_db_select_tent_part(newdt, pel->path, pel->len, &tent) < 0)
			return -1;
		free(pel);
		if(!tent) {
			struct mapping *map;

			fprintf(f, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output\n", w->filename);
			write_bork = 1;
#ifdef _WIN32
			/* t5038 - Need to clean up files until Windows supports tmpfiles properly. */
			fprintf(f, "[35m -- Delete: %s[0m\n", w->filename);
			unlink(w->filename);
#endif

			LIST_FOREACH(map, &info->mapping_list, list) {
				if(strcmp(map->realname, w->filename) == 0) {
					del_map(map);
					break;
				}
			}
		} else {
			struct mapping *map;
			tup_entry_list_add(tent, entryhead);

			LIST_FOREACH(map, &info->mapping_list, list) {
				if(strcmp(map->realname, w->filename) == 0) {
					map->tent = tent;
				}
			}
		}

out_skip:
		del_file_entry(w);
	}

	if(tup_db_check_actual_outputs(f, cmdid, entryhead, &info->mapping_list, &write_bork) < 0)
		return -1;

	while(!LIST_EMPTY(&info->mapping_list)) {
		struct mapping *map;

		map = LIST_FIRST(&info->mapping_list);

		/* TODO: strcmp only here for win32 support */
		if(strcmp(map->tmpname, map->realname) != 0) {
			if(renameat(tup_top_fd(), map->tmpname, tup_top_fd(), map->realname) < 0) {
				perror(map->realname);
				fprintf(f, "tup error: Unable to rename temporary file '%s' to destination '%s'\n", map->tmpname, map->realname);
				write_bork = 1;
			}
		}
		if(map->tent) {
			/* tent may not be set (in the case of hidden files) */
			if(file_set_mtime(map->tent, map->realname) < 0)
				return -1;
		} else {
			printf(" [31m - NO TENT[0m\n");
		}
		del_map(map);
	}

	if(write_bork)
		return -1;

	return 0;
}

static int update_read_info(FILE *f, tupid_t cmdid, struct file_info *info,
			    struct tup_entry_head *entryhead,
			    struct tupid_entries *sticky_root,
			    struct tupid_entries *normal_root,
			    struct tupid_entries *group_sticky_root,
			    int full_deps, tupid_t vardt,
			    struct tupid_entries *used_groups_root,
			    int *important_link_removed)
{
	struct file_entry *r;
	struct tupid_tree *tt;

	while(!LIST_EMPTY(&info->read_list)) {
		r = LIST_FIRST(&info->read_list);

		if(add_node_to_list(f, DOT_DT, &r->pg, entryhead, full_deps, r->filename) < 0)
			return -1;
		del_file_entry(r);
	}

	while(!LIST_EMPTY(&info->var_list)) {
		r = LIST_FIRST(&info->var_list);

		if(add_node_to_list(f, vardt, &r->pg, entryhead, 0, NULL) < 0)
			return -1;
		del_file_entry(r);
	}

	RB_FOREACH(tt, tupid_entries, used_groups_root) {
		struct tup_entry *tent;
		if(tup_entry_add(tt->tupid, &tent) < 0)
			return -1;
		tup_entry_list_add(tent, entryhead);
	}

	if(tup_db_check_actual_inputs(f, cmdid, entryhead, sticky_root, normal_root, group_sticky_root, important_link_removed) < 0)
		return -1;
	return 0;
}
