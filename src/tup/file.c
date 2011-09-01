#define _ATFILE_SOURCE
#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "linux/list.h"
#include "db.h"
#include "fileio.h"
#include "pel_group.h"
#include "config.h"
#include "entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct file_entry {
	tupid_t dt;
	char *filename;
	struct pel_group pg;
	struct list_head list;
};

struct dfd_info {
	tupid_t dt;
	int dfd;
};

static struct file_entry *new_entry(const char *filename, tupid_t dt);
static void del_entry(struct file_entry *fent);
static void check_unlink_list(const struct pel_group *pg, struct list_head *u_list);
static void handle_unlink(struct file_info *info);
static int update_write_info(tupid_t cmdid, const char *debug_name,
			     struct file_info *info, int *warnings,
			     struct list_head *entrylist);
static int update_read_info(tupid_t cmdid, struct file_info *info,
			    struct list_head *entrylist);
static int add_parser_files_locked(struct file_info *finfo, struct rb_root *root);

int init_file_info(struct file_info *info)
{
	INIT_LIST_HEAD(&info->read_list);
	INIT_LIST_HEAD(&info->write_list);
	INIT_LIST_HEAD(&info->unlink_list);
	INIT_LIST_HEAD(&info->var_list);
	INIT_LIST_HEAD(&info->mapping_list);
	INIT_LIST_HEAD(&info->tmpdir_list);
	pthread_mutex_init(&info->lock, NULL);
	info->server_fail = 0;
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
		struct file_info *info, tupid_t dt)
{
	DEBUGP("received file '%s' in mode %i\n", filename, at);
	int rc;

	finfo_lock(info);
	if(at == ACCESS_RENAME) {
		rc = handle_rename(filename, file2, info);
	} else {
		rc = handle_open_file(at, filename, info, dt);
	}
	finfo_unlock(info);

	return rc;
}

int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info, tupid_t dt)
{
	struct file_entry *fent;
	int rc = 0;

	fent = new_entry(filename, dt);
	if(!fent) {
		return -1;
	}

	switch(at) {
		case ACCESS_READ:
			list_add(&fent->list, &info->read_list);
			break;
		case ACCESS_WRITE:
			check_unlink_list(&fent->pg, &info->unlink_list);
			list_add(&fent->list, &info->write_list);
			break;
		case ACCESS_UNLINK:
			list_add(&fent->list, &info->unlink_list);
			break;
		case ACCESS_VAR:
			list_add(&fent->list, &info->var_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(tupid_t cmdid, const char *debug_name, struct file_info *info,
		int *warnings)
{
	struct list_head *entrylist;
	struct tmpdir *tmpdir;
	int tmpdir_bork = 0;
	int rc1, rc2;

	finfo_lock(info);
	handle_unlink(info);

	list_for_each_entry(tmpdir, &info->tmpdir_list, list) {
		fprintf(stderr, "tup error: Directory '%s' was created by command '%s', but not subsequently removed. Only temporary directories can be created by commands.\n", tmpdir->dirname, debug_name);
		tmpdir_bork = 1;
	}
	if(tmpdir_bork) {
		finfo_unlock(info);
		return -1;
	}

	entrylist = tup_entry_get_list();
	rc1 = update_write_info(cmdid, debug_name, info, warnings, entrylist);
	tup_entry_release_list();

	entrylist = tup_entry_get_list();
	rc2 = update_read_info(cmdid, info, entrylist);
	tup_entry_release_list();
	finfo_unlock(info);

	if(rc1 == 0 && rc2 == 0)
		return 0;
	return -1;
}

int add_parser_files(struct file_info *finfo, struct rb_root *root)
{
	int rc;
	finfo_lock(finfo);
	rc = add_parser_files_locked(finfo, root);
	finfo_unlock(finfo);
	return rc;
}

static int add_parser_files_locked(struct file_info *finfo, struct rb_root *root)
{
	struct file_entry *r;
	struct mapping *map;
	struct list_head *entrylist;
	struct tup_entry *tent;
	int map_bork = 0;

	entrylist = tup_entry_get_list();
	while(!list_empty(&finfo->read_list)) {
		r = list_entry(finfo->read_list.next, struct file_entry, list);
		if(r->dt > 0) {
			if(add_node_to_list(r->dt, &r->pg, entrylist) < 0)
				return -1;
		}
		del_entry(r);
	}
	while(!list_empty(&finfo->var_list)) {
		r = list_entry(finfo->var_list.next, struct file_entry, list);

		if(add_node_to_list(VAR_DT, &r->pg, entrylist) < 0)
			return -1;
		del_entry(r);
	}
	list_for_each_entry(tent, entrylist, list) {
		if(strcmp(tent->name.s, ".gitignore") != 0)
			if(tupid_tree_add_dup(root, tent->tnode.tupid) < 0)
				return -1;
	}
	tup_entry_release_list();

	/* TODO: write_list not needed here? */
	while(!list_empty(&finfo->write_list)) {
		r = list_entry(finfo->write_list.next, struct file_entry, list);
		del_entry(r);
	}

	while(!list_empty(&finfo->mapping_list)) {
		map = list_entry(finfo->mapping_list.next, struct mapping, list);

		if(gimme_tent(map->realname, &tent) < 0)
			return -1;
		if(!tent || strcmp(tent->name.s, ".gitignore") != 0) {
			fprintf(stderr, "tup error: Writing to file '%s' while parsing is not allowed. Only a .gitignore file may be created during the parsing stage.\n", map->realname);
			map_bork = 1;
		} else {
			if(renameat(tup_top_fd(), map->tmpname, tup_top_fd(), map->realname) < 0) {
				perror("renameat");
				return -1;
			}
		}
		del_map(map);
	}
	if(map_bork)
		return -1;
	return 0;
}

static int file_set_mtime(struct tup_entry *tent, int dfd, const char *file)
{
	struct stat buf;
	if(fstatat(dfd, file, &buf, AT_SYMLINK_NOFOLLOW) < 0) {
		fprintf(stderr, "tup error: file_set_mtime() fstatat failed.\n");
		perror(file);
		return -1;
	}
	if(tup_db_set_mtime(tent, buf.st_mtime) < 0)
		return -1;
	return 0;
}

static struct file_entry *new_entry(const char *filename, tupid_t dt)
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
	fent->dt = dt;
	return fent;
}

static void del_entry(struct file_entry *fent)
{
	list_del(&fent->list);
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

	list_for_each_entry(fent, &info->write_list, list) {
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
	list_for_each_entry(fent, &info->read_list, list) {
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
	list_del(&map->list);
	free(map->tmpname);
	free(map->realname);
	free(map);
}

static void check_unlink_list(const struct pel_group *pg, struct list_head *u_list)
{
	struct file_entry *fent, *tmp;

	list_for_each_entry_safe(fent, tmp, u_list, list) {
		if(pg_eq(&fent->pg, pg)) {
			del_entry(fent);
		}
	}
}

static void handle_unlink(struct file_info *info)
{
	struct file_entry *u, *fent, *tmp;

	while(!list_empty(&info->unlink_list)) {
		u = list_entry(info->unlink_list.next, struct file_entry, list);

		list_for_each_entry_safe(fent, tmp, &info->write_list, list) {
			if(pg_eq(&fent->pg, &u->pg)) {
				del_entry(fent);
			}
		}
		list_for_each_entry_safe(fent, tmp, &info->read_list, list) {
			if(pg_eq(&fent->pg, &u->pg)) {
				del_entry(fent);
			}
		}

		del_entry(u);
	}
}

static int update_write_info(tupid_t cmdid, const char *debug_name,
			     struct file_info *info, int *warnings,
			     struct list_head *entrylist)
{
	struct file_entry *w;
	struct file_entry *r;
	struct file_entry *tmp;
	struct tup_entry *tent;
	int write_bork = 0;

	while(!list_empty(&info->write_list)) {
		tupid_t newdt;
		struct path_element *pel = NULL;

		w = list_entry(info->write_list.next, struct file_entry, list);
		if(w->dt < 0) {
			goto out_skip;
		}

		/* Remove duplicate write entries */
		list_for_each_entry_safe(r, tmp, &info->write_list, list) {
			if(r != w && pg_eq(&w->pg, &r->pg)) {
				del_entry(r);
			}
		}

		if(w->pg.pg_flags & PG_HIDDEN) {
			fprintf(stderr, "tup warning: Writing to hidden file '%s' from command '%s'\n", w->filename, debug_name);
			(*warnings)++;
			goto out_skip;
		}

		newdt = find_dir_tupid_dt_pg(w->dt, &w->pg, &pel, 0);
		if(newdt <= 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for the command '%s'\n", w->filename, debug_name);
			return -1;
		}
		if(!pel) {
			fprintf(stderr, "[31mtup internal error: find_dir_tupid_dt_pg() in write_files() didn't get a final pel pointer.[0m\n");
			return -1;
		}

		if(tup_db_select_tent_part(newdt, pel->path, pel->len, &tent) < 0)
			return -1;
		free(pel);
		if(!tent) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for the command '%s'\n", w->filename, debug_name);
			write_bork = 1;
		} else {
			struct mapping *map;
			int dfd;
			tup_entry_list_add(tent, entrylist);

			/* Some files in Windows still set dt to not be
			 * DOT_DT, so we need to make sure we are in the
			 * right path for fstatat() to work. The fuse
			 * server always sets dt to DOT_DT, so we can just
			 * use the existing tup_top_fd() descriptor in that
			 * case.
			 */
			if(w->dt != DOT_DT) {
				dfd = tup_entry_open_tupid(w->dt);
			} else {
				dfd = tup_top_fd();
			}

			list_for_each_entry(map, &info->mapping_list, list) {
				if(strcmp(map->realname, w->filename) == 0) {
					if(file_set_mtime(tent, dfd, map->tmpname) < 0)
						return -1;
				}
			}
			if(w->dt != DOT_DT) {
				close(dfd);
			}
		}

out_skip:
		del_entry(w);
	}

	if(write_bork) {
		while(!list_empty(&info->mapping_list)) {
			struct mapping *map;

			map = list_entry(info->mapping_list.next, struct mapping, list);
			unlink(map->tmpname);
			del_map(map);
		}
		return -1;
	}

	if(tup_db_check_actual_outputs(cmdid, entrylist) < 0)
		return -1;

	while(!list_empty(&info->mapping_list)) {
		struct mapping *map;

		map = list_entry(info->mapping_list.next, struct mapping, list);

		/* TODO: strcmp only here for win32 support */
		if(strcmp(map->tmpname, map->realname) != 0) {
			if(renameat(tup_top_fd(), map->tmpname, tup_top_fd(), map->realname) < 0) {
				perror(map->realname);
				fprintf(stderr, "tup error: Unable to rename temporary file '%s' to destination '%s'\n", map->tmpname, map->realname);
				write_bork = 1;
			}
		}
		del_map(map);
	}

	if(write_bork)
		return -1;

	return 0;
}

static int update_read_info(tupid_t cmdid, struct file_info *info,
			    struct list_head *entrylist)
{
	struct file_entry *r;

	while(!list_empty(&info->read_list)) {
		r = list_entry(info->read_list.next, struct file_entry, list);
		if(r->dt > 0) {
			if(add_node_to_list(r->dt, &r->pg, entrylist) < 0)
				return -1;
		}
		del_entry(r);
	}

	while(!list_empty(&info->var_list)) {
		r = list_entry(info->var_list.next, struct file_entry, list);

		if(add_node_to_list(VAR_DT, &r->pg, entrylist) < 0)
			return -1;
		del_entry(r);
	}

	if(tup_db_check_actual_inputs(cmdid, entrylist) < 0)
		return -1;
	return 0;
}
