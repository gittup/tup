/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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
#include <errno.h>
#include <sys/stat.h>

static struct file_entry *new_entry(const char *filename);
static void check_unlink_list(const char *filename, struct file_entry_head *u_head);
static void handle_unlink(struct file_info *info);
static int update_write_info(FILE *f, tupid_t cmdid, struct file_info *info,
			     int *warnings, struct tup_entry_head *entryhead,
			     enum check_type_t check_only);
static int update_read_info(FILE *f, tupid_t cmdid, struct file_info *info,
			    struct tup_entry_head *entryhead,
			    int full_deps, tupid_t vardt,
			    int *important_link_removed);
static int add_config_files_locked(struct file_info *finfo, struct tup_entry *tent);
static int add_parser_files_locked(struct file_info *finfo,
				   struct tupid_entries *root, tupid_t vardt);

int init_file_info(struct file_info *info, const char *variant_dir, int do_unlink)
{
	LIST_INIT(&info->read_list);
	LIST_INIT(&info->write_list);
	LIST_INIT(&info->unlink_list);
	LIST_INIT(&info->var_list);
	LIST_INIT(&info->mapping_list);
	LIST_INIT(&info->tmpdir_list);
	RB_INIT(&info->sticky_root);
	RB_INIT(&info->normal_root);
	RB_INIT(&info->group_sticky_root);
	RB_INIT(&info->used_groups_root);
	RB_INIT(&info->output_root);
	LIST_INIT(&info->exclusion_list);
	pthread_mutex_init(&info->lock, NULL);
	pthread_cond_init(&info->cond, NULL);
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
	info->do_unlink = do_unlink;
	return 0;
}

void cleanup_file_info(struct file_info *info)
{
	free_re_list(&info->exclusion_list);
	free_tupid_tree(&info->output_root);
	free_tupid_tree(&info->used_groups_root);
	free_tupid_tree(&info->group_sticky_root);
	free_tupid_tree(&info->normal_root);
	free_tupid_tree(&info->sticky_root);
}

void finfo_lock(struct file_info *info)
{
	pthread_mutex_lock(&info->lock);
}

void finfo_unlock(struct file_info *info)
{
	pthread_mutex_unlock(&info->lock);
}

int handle_file_dtent(enum access_type at, struct tup_entry *dtent,
		      const char *filename, struct file_info *info)
{
	if(is_full_path(filename)) {
		return handle_file(at, filename, "", info);
	} else {
		if(dtent->tnode.tupid == DOT_DT) {
			return handle_file(at, filename, "", info);
		} else {
			char fullname[PATH_MAX];
			int rc;

			fullname[0] = '.';
			rc = snprint_tup_entry(fullname+1, sizeof(fullname)-1, dtent);
			if(rc >= PATH_MAX) {
				fprintf(stderr, "tup error: string size too small in handle_file_dtent\n");
				return -1;
			}
			if(snprintf(fullname+rc+1, sizeof(fullname)-rc-1, "/%s", filename) >= PATH_MAX) {
				fprintf(stderr, "tup error: string size too small in handle_file_dtent\n");
				return -1;
			}
			return handle_file(at, fullname, "", info);
		}
	}
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
			check_unlink_list(filename, &info->unlink_list);
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
		enum check_type_t check_only,
		int full_deps, tupid_t vardt,
		int *important_link_removed)
{
	struct tup_entry_head *entrylist;
	struct tmpdir *tmpdir;
	int tmpdir_bork = 0;
	int rc1 = 0, rc2 = 0;

	finfo_lock(info);
	handle_unlink(info);

	if(check_only == CHECK_SUCCESS) {
		while(!LIST_EMPTY(&info->tmpdir_list)) {
			int match = 0;

			tmpdir = LIST_FIRST(&info->tmpdir_list);
			if(re_entries_match(f, &info->exclusion_list, tmpdir->dirname, &match) < 0)
				return -1;
			if(match) {
				if(mkdir(tmpdir->dirname, 0777) < 0) {
					if(errno != EEXIST) {
						perror(tmpdir->dirname);
						fprintf(f, "tup error: Unable to create directory '%s'\n", tmpdir->dirname);
						return -1;
					}
				}
			} else {
				fprintf(f, "tup error: Directory '%s' was created, but not subsequently removed. Only temporary directories can be created by commands.\n", tmpdir->dirname);
				tmpdir_bork = 1;
			}
			LIST_REMOVE(tmpdir, list);
			free(tmpdir->dirname);
			free(tmpdir);
		}
		if(tmpdir_bork) {
			finfo_unlock(info);
			return -1;
		}
	}

	entrylist = tup_entry_get_list();
	rc1 = update_write_info(f, cmdid, info, warnings, entrylist, check_only);
	tup_entry_release_list();

	/* Only process file inputs if the command wasn't signaled. We
	 * obviously need to check them if the command succeeded to ensure DAG
	 * consistency, and they can also help to check when the command
	 * returns a failure code (ie: if a header file was missing because it
	 * hasn't been generated yet since we didn't declare it as an input).
	 * When the command fails due to a signal, however, there's no value
	 * in checking file inputs since presumably the user just wants to get
	 * back to the commandline ASAP.
	 */
	if(check_only != CHECK_SIGNALLED) {
		entrylist = tup_entry_get_list();
		rc2 = update_read_info(f, cmdid, info, entrylist, full_deps, vardt, important_link_removed);
		tup_entry_release_list();
	}
	finfo_unlock(info);

	if(rc1 == 0 && rc2 == 0)
		return 0;
	if(rc2 < 0) {
		if(check_only == CHECK_SUCCESS) {
			if(rc1 < 0) {
				fprintf(f, " *** Command ran successfully, but failed due to errors processing both the input and output dependencies.\n");
			} else {
				fprintf(f, " *** Command ran successfully, but failed due to errors processing input dependencies.\n");
			}
		} else {
			fprintf(f, " *** Additionally, the command failed to process input dependencies. These should probably be fixed before addressing the command failure.\n");
		}
	} else {
		if(rc1 < 0) {
			/* Only print file write error if the rest succeeded.
			 * We generally expect not to write the output files if
			 * the command fails.
			 */
			if(check_only == CHECK_SUCCESS) {
				fprintf(f, " *** Command failed due to errors processing the output dependencies.\n");
			}
		}
	}
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

int add_parser_files(struct file_info *finfo, struct tupid_entries *root, tupid_t vardt)
{
	int rc;
	finfo_lock(finfo);
	rc = add_parser_files_locked(finfo, root, vardt);
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

static int add_node_to_list(tupid_t dt, const char *filename,
			    struct tup_entry_head *head, int full_deps, const char *full_path)
{
	tupid_t new_dt;
	struct path_element *pel = NULL;
	struct pel_group pg;
	struct tup_entry *tent;

	if(get_path_elements(filename, &pg) < 0)
		return -1;

	new_dt = find_dir_tupid_dt_pg(dt, &pg, &pel, 1, full_deps);
	if(new_dt < 0)
		return -1;
	if(new_dt == 0) {
		del_pel_group(&pg);
		return 0;
	}
	if(pel == NULL) {
		/* This can happen for the '.' entry */
		del_pel_group(&pg);
		return 0;
	}

	if(tup_db_select_tent_part(new_dt, pel->path, pel->len, &tent) < 0)
		return -1;
	if(!tent) {
		time_t mtime = -1;
		if(full_deps && (pg.pg_flags & PG_OUTSIDE_TUP)) {
			struct stat buf;
			if(lstat(full_path, &buf) == 0) {
				mtime = MTIME(buf);
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
	del_pel_group(&pg);

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

	return 0;
}

#ifdef _WIN32
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

static unsigned filetime_to_epoch(FILETIME *ft)
{
        long long ticks = (((long long)ft->dwHighDateTime) << 32) + (long long)ft->dwLowDateTime;
        return (unsigned)(ticks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

static int file_set_mtime(struct tup_entry *tent, const char *file)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	wchar_t widefile[WIDE_PATH_MAX];
	int prefix_len = 4;
	wchar_t *dest;
	wchar_t *tmp;

	dest = widefile;
	if(is_full_path(file)) {
		/* Everything from the DLL injection is a full path, but we
		 * need to prefix with \\?\ make GetFileAttributesEx work with
		 * >260 character paths.
		 */
		wcscpy(widefile, L"\\\\?\\");
		dest += prefix_len;
	}
	MultiByteToWideChar(CP_UTF8, 0, file, -1, dest, WIDE_PATH_MAX - prefix_len);
	widefile[WIDE_PATH_MAX-1] = 0;
	/* Backout the forward-slash conversion used for regex matching. */
	for(tmp=dest; *tmp != 0; tmp++) {
		if(*tmp == '/') {
			*tmp = '\\';
		}
	}
	if(!GetFileAttributesExW(widefile, GetFileExInfoStandard, &data)) {
		fprintf(stderr, "tup error: GetFileAttributesExW(\"%ls\") failed: 0x%08lx\n", widefile, GetLastError());
		return -1;
	}
	if(tup_db_set_mtime(tent, filetime_to_epoch(&data.ftLastWriteTime)) < 0)
		return -1;
	return 0;
}
#else
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
	if(tup_db_set_mtime(tent, MTIME(buf)) < 0)
		return -1;
	return 0;
}
#endif

static int add_config_files_locked(struct file_info *finfo, struct tup_entry *tent)
{
	struct file_entry *r;
	struct tup_entry_head *entrylist;
	int full_deps = tup_option_get_int("updater.full_deps");

	entrylist = tup_entry_get_list();
	while(!LIST_EMPTY(&finfo->read_list)) {
		struct tup_entry *tmp;
		r = LIST_FIRST(&finfo->read_list);

		if(add_node_to_list(DOT_DT, r->filename, entrylist, full_deps, r->filename) < 0)
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

static int add_parser_files_locked(struct file_info *finfo,
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
		if(add_node_to_list(DOT_DT, r->filename, entrylist, full_deps, r->filename) < 0)
			return -1;
		del_file_entry(r);
	}
	while(!LIST_EMPTY(&finfo->var_list)) {
		r = LIST_FIRST(&finfo->var_list);

		if(add_node_to_list(vardt, r->filename, entrylist, 0, NULL) < 0)
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
	return fent;
}

void del_file_entry(struct file_entry *fent)
{
	LIST_REMOVE(fent, list);
	free(fent->filename);
	free(fent);
}

int handle_rename(const char *from, const char *to, struct file_info *info)
{
	struct file_entry *fent;

	LIST_FOREACH(fent, &info->write_list, list) {
		if(strcmp(fent->filename, from) == 0) {
			free(fent->filename);

			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	LIST_FOREACH(fent, &info->read_list, list) {
		if(strcmp(fent->filename, from) == 0) {
			free(fent->filename);

			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}

	check_unlink_list(to, &info->unlink_list);
	return 0;
}

void del_map(struct mapping *map)
{
	LIST_REMOVE(map, list);
	free(map->tmpname);
	free(map->realname);
	free(map);
}

static void check_unlink_list(const char *filename, struct file_entry_head *u_head)
{
	struct file_entry *fent, *tmp;

	LIST_FOREACH_SAFE(fent, u_head, list, tmp) {
		if(strcmp(filename, fent->filename) == 0) {
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
			if(strcmp(fent->filename, u->filename) == 0) {
				del_file_entry(fent);
			}
		}
		LIST_FOREACH_SAFE(fent, &info->read_list, list, tmp) {
			if(strcmp(fent->filename, u->filename) == 0) {
				del_file_entry(fent);
			}
		}

		del_file_entry(u);
	}
}

static int create_ignored_file(FILE *f, struct file_entry *w)
{
	struct path_element *pel = NULL;
	struct tup_entry *tent;
	tupid_t dt;

	dt = find_dir_tupid_dt(DOT_DT, w->filename, &pel, SOTGV_IGNORE_DIRS, 0);
	if(dt < 0) {
		fprintf(f, "tup error: Unable to create directory tree for ignored file: %s\n", w->filename);
		return -1;
	}
	if(!pel) {
		fprintf(f, "tup internal error: create_ignored_file() didn't get a final pel pointer for file: %s\n", w->filename);
		return -1;
	}
	tent = tup_db_create_node_part(dt, pel->path, pel->len, TUP_NODE_FILE, -1, NULL);
	if(!tent)
		return -1;
	free(pel);
	return 0;
}

static int update_write_info(FILE *f, tupid_t cmdid, struct file_info *info,
			     int *warnings, struct tup_entry_head *entryhead,
			     enum check_type_t check_only)
{
	struct file_entry *w;
	struct file_entry *r;
	struct file_entry *tmp;
	struct tup_entry *tent;
	int write_bork = 0;

	while(!LIST_EMPTY(&info->write_list)) {
		tupid_t newdt;
		struct path_element *pel = NULL;
		struct pel_group pg;
		int match = 0;

		w = LIST_FIRST(&info->write_list);

		if(re_entries_match(f, &info->exclusion_list, w->filename, &match) < 0)
			return -1;
		if(match) {
			if(create_ignored_file(f, w) < 0)
				return -1;
			goto out_skip;
		}

		/* Remove duplicate write entries */
		LIST_FOREACH_SAFE(r, &info->write_list, list, tmp) {
			if(r != w && (strcmp(w->filename, r->filename) == 0)) {
				del_file_entry(r);
			}
		}

		if(get_path_elements(w->filename, &pg) < 0)
			return -1;
		if(pg.pg_flags & PG_HIDDEN) {
			if(warnings) {
				fprintf(f, "tup warning: Writing to hidden file '%s'\n", w->filename);
				(*warnings)++;
			}
			del_pel_group(&pg);
			goto out_skip;
		}

		tent = NULL;
		newdt = find_dir_tupid_dt_pg(DOT_DT, &pg, &pel, 0, 0);
		del_pel_group(&pg);
		if(newdt > 0) {
			if(!pel) {
				fprintf(f, "tup internal error: find_dir_tupid_dt_pg() in write_files() didn't get a final pel pointer for file: %s\n", w->filename);
				return -1;
			}

			if(tup_db_select_tent_part(newdt, pel->path, pel->len, &tent) < 0)
				return -1;
			free(pel);
		}
		if(!tent) {
			struct mapping *map;

			fprintf(f, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output\n", w->filename);
			write_bork = 1;
			if(info->do_unlink) {
				fprintf(f, " -- Delete: %s\n", w->filename);
				unlink(w->filename);
			}

			LIST_FOREACH(map, &info->mapping_list, list) {
				if(strcmp(map->realname, w->filename) == 0) {
					del_map(map);
					break;
				}
			}
		} else {
			struct mapping *map;
			int mapping_set = 0;
			tup_entry_list_add(tent, entryhead);

			LIST_FOREACH(map, &info->mapping_list, list) {
				if(strcmp(map->realname, w->filename) == 0) {
					map->tent = tent;
					mapping_set = 1;
				}
			}
			if(!mapping_set) {
				map = malloc(sizeof *map);
				if(!map) {
					perror("malloc");
					return -1;
				}
				map->realname = strdup(w->filename);
				if(!map->realname) {
					perror("strdup");
					return -1;
				}
				map->tmpname = strdup(w->filename);
				if(!map->tmpname) {
					perror("strdup");
					return -1;
				}
				map->tent = tent;
				LIST_INSERT_HEAD(&info->mapping_list, map, list);
			}
		}

out_skip:
		del_file_entry(w);
	}

	if(tup_db_check_actual_outputs(f, cmdid, entryhead, &info->output_root, &info->mapping_list, &write_bork, info->do_unlink, check_only==CHECK_SUCCESS) < 0)
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
		}
		del_map(map);
	}

	if(write_bork)
		return -1;

	return 0;
}

static int update_read_info(FILE *f, tupid_t cmdid, struct file_info *info,
			    struct tup_entry_head *entryhead,
			    int full_deps, tupid_t vardt,
			    int *important_link_removed)
{
	struct file_entry *r;
	struct tupid_tree *tt;

	while(!LIST_EMPTY(&info->read_list)) {
		int match = 0;
		r = LIST_FIRST(&info->read_list);

		if(re_entries_match(f, &info->exclusion_list, r->filename, &match) < 0)
			return -1;
		if(!match) {
			if(add_node_to_list(DOT_DT, r->filename, entryhead, full_deps, r->filename) < 0)
				return -1;
		}
		del_file_entry(r);
	}

	while(!LIST_EMPTY(&info->var_list)) {
		r = LIST_FIRST(&info->var_list);

		if(add_node_to_list(vardt, r->filename, entryhead, 0, NULL) < 0)
			return -1;
		del_file_entry(r);
	}

	RB_FOREACH(tt, tupid_entries, &info->used_groups_root) {
		struct tup_entry *tent;
		if(tup_entry_add(tt->tupid, &tent) < 0)
			return -1;
		tup_entry_list_add(tent, entryhead);
	}

	if(tup_db_check_actual_inputs(f, cmdid, entryhead, &info->sticky_root, &info->normal_root, &info->group_sticky_root, &info->output_root, important_link_removed) < 0)
		return -1;
	return 0;
}
