/* _ATFILE_SOURCE for fstatat */
#define _ATFILE_SOURCE
#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "linux/list.h"
#include "db.h"
#include "fileio.h"
#include "entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

struct file_entry {
	char *filename;
	struct pel_group pg;
	struct list_head list;
};

struct sym_entry {
	char *from;
	char *to;
	struct list_head list;
};

static struct file_entry *new_entry(const char *filename);
static void del_entry(struct file_entry *fent);
static int handle_rename(const char *from, const char *to,
			 struct file_info *info);
static int handle_symlink(const char *from, const char *to,
			  struct file_info *info);
static void check_unlink_list(const struct pel_group *pg, struct list_head *u_list);
static void handle_unlink(struct file_info *info);

int init_file_info(struct file_info *info)
{
	INIT_LIST_HEAD(&info->read_list);
	INIT_LIST_HEAD(&info->write_list);
	INIT_LIST_HEAD(&info->unlink_list);
	INIT_LIST_HEAD(&info->var_list);
	INIT_LIST_HEAD(&info->sym_list);
	INIT_LIST_HEAD(&info->ghost_list);
	return 0;
}

int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("received file '%s' in mode %i\n", filename, at);

	if(at == ACCESS_RENAME) {
		return handle_rename(filename, file2, info);
	}
	if(at == ACCESS_SYMLINK) {
		return handle_symlink(filename, file2, info);
	}

	fent = new_entry(filename);
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
		case ACCESS_GHOST:
			list_add(&fent->list, &info->ghost_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(tupid_t cmdid, tupid_t dt, int dfd, const char *debug_name,
		struct file_info *info, int *warnings)
{
	struct file_entry *w;
	struct file_entry *r;
	struct file_entry *g;
	struct file_entry *tmp;
	struct db_node dbn;
	struct tup_entry *tent;
	int write_bork = 0;
	struct list_head *readlist;
	struct list_head *symlist;

	handle_unlink(info);

	while(!list_empty(&info->write_list)) {
		tupid_t newdt;
		struct path_element *pel = NULL;

		w = list_entry(info->write_list.next, struct file_entry, list);

		/* Some programs read from a file before writing over it. In
		 * this case we don't want to have a link both to and from the
		 * command, and writing takes precedence.
		 */
		list_for_each_entry_safe(r, tmp, &info->read_list, list) {
			if(pg_eq(&w->pg, &r->pg))
				del_entry(r);
		}
		list_for_each_entry_safe(g, tmp, &info->ghost_list, list) {
			if(pg_eq(&w->pg, &g->pg))
				del_entry(g);
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

		symlist = tup_entry_get_list();
		newdt = find_dir_tupid_dt_pg(dt, &w->pg, &pel, symlist, 0);
		if(newdt <= 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for the command '%s'\n", w->filename, debug_name);
			return -1;
		}
		if(!pel) {
			fprintf(stderr, "[31mtup internal error: find_dir_tupid_dt_pg() in write_files() didn't get a final pel pointer.[0m\n");
			return -1;
		}
		if(!list_empty(symlist)) {
			fprintf(stderr, "tup error: Attempt to write to a file using a symlink. The command should only  use the full non-symlinked path, or just write to the current directory.\n");
			fprintf(stderr, " -- Command: '%s'\n", debug_name);
			fprintf(stderr, " -- Filename: '%s'\n", w->filename);
			list_for_each_entry(tent, symlist, list) {
				fprintf(stderr, " -- Symlink %lli -> %lli in dir %lli\n", tent->tnode.tupid, tent->sym, tent->dt);
			}
			return -1;
		}
		tup_entry_release_list();

		if(tup_db_select_dbn_part(newdt, pel->path, pel->len, &dbn) < 0)
			return -1;
		/* Don't need to follow the syms of  dbn here, since the output
		 * file was removed by the updater. So our database
		 * representation may not match the filesystem, untilwe reset
		 * the sym field to -1 later.
		 */
		free(pel);
		if(dbn.tupid < 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for the command '%s'\n", w->filename, debug_name);
			fprintf(stderr, " Unlink: [35m%s[0m\n", w->filename);
			unlinkat(dfd, w->filename, 0);
			write_bork = 1;
		} else {
			if(tup_db_add_write_list(dbn.tupid) < 0)
				return -1;
			if(file_set_mtime(dbn.tupid, dfd, w->filename) < 0)
				return -1;
			if(tup_db_set_sym(dbn.tupid, -1) < 0)
				return -1;
		}

out_skip:
		del_entry(w);
	}

	while(!list_empty(&info->sym_list)) {
		struct sym_entry *sym_entry;
		struct tup_entry *link_tent;
		tupid_t sym;

		sym_entry = list_entry(info->sym_list.next, struct sym_entry, list);

		if(tup_db_select_dbn(dt, sym_entry->to, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			int dirfd;
			fprintf(stderr, "tup error: File '%s' was written as a symlink, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", sym_entry->to, debug_name);
			fprintf(stderr, " Unlink: [35m%s[0m\n", sym_entry->to);
			dirfd = tup_entry_open_tupid(dt);
			if(dirfd < 0) {
				fprintf(stderr, "Unable to automatically unlink file.\n");
			} else {
				unlinkat(dirfd, sym_entry->to, 0);
				close(dirfd);
			}
			write_bork = 1;
			goto skip_sym;
		}

		if(tup_db_add_write_list(dbn.tupid) < 0)
			return -1;
		if(file_set_mtime(dbn.tupid, dfd, sym_entry->to) < 0)
			return -1;

		list_for_each_entry_safe(g, tmp, &info->ghost_list, list) {
			/* Use strcmp instead of pg_eq because we don't have
			 * the pgs for sym_entries. Also this should only
			 * happen when 'ln' does a stat() before it does a
			 * symlink().
			 */
			if(strcmp(sym_entry->to, g->filename) == 0)
				del_entry(g);
		}

		/* Don't pass in readlist for the list parameter - we don't
		 * actually need to track symlinks referenced by the path of
		 * the symlink file. These would get picked up by any command
		 * that reads our symlink.
		 */
		if(gimme_node_or_make_ghost(dt, sym_entry->from, NULL, &link_tent) < 0)
			return -1;
		if(link_tent) {
			sym = link_tent->tnode.tupid;
		} else {
			sym = -1;
		}

		if(tup_db_set_sym(dbn.tupid, sym) < 0)
			return -1;

skip_sym:
		list_del(&sym_entry->list);
		free(sym_entry->from);
		free(sym_entry->to);
		free(sym_entry);
	}

	if(write_bork)
		return -1;

	if(tup_db_check_write_list(cmdid) < 0)
		return -1;
	if(tup_db_clear_tmp_list() < 0)
		return -1;

	readlist = tup_entry_get_list();
	while(!list_empty(&info->read_list)) {
		r = list_entry(info->read_list.next, struct file_entry, list);

		if(add_node_to_list(dt, &r->pg, readlist, 0) < 0)
			return -1;
		del_entry(r);
	}

	while(!list_empty(&info->var_list)) {
		r = list_entry(info->var_list.next, struct file_entry, list);

		if(add_node_to_list(VAR_DT, &r->pg, readlist, 1) < 0)
			return -1;
		del_entry(r);
	}

	while(!list_empty(&info->ghost_list)) {
		g = list_entry(info->ghost_list.next, struct file_entry, list);

		if(add_node_to_list(dt, &g->pg, readlist, 1) < 0)
			return -1;
		del_entry(g);
	}
	if(tup_db_check_actual_inputs(cmdid, readlist) < 0)
		return -1;
	tup_entry_release_list();

	return 0;
}

int file_set_mtime(tupid_t tupid, int dfd, const char *file)
{
	struct stat buf;
	if(fstatat(dfd, file, &buf, AT_SYMLINK_NOFOLLOW) != 0) {
		fprintf(stderr, "tup error: file_set_mtime() fstatat failed.\n");
		perror(file);
		return -1;
	}
	if(tup_db_set_mtime(tupid, buf.st_mtime) < 0)
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
		return NULL;
	}

	if(get_path_elements(fent->filename, &fent->pg) < 0) {
		return NULL;
	}
	return fent;
}

static void del_entry(struct file_entry *fent)
{
	list_del(&fent->list);
	del_pel_list(&fent->pg.path_list);
	free(fent->filename);
	free(fent);
}

static int handle_rename(const char *from, const char *to,
			 struct file_info *info)
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
			del_pel_list(&fent->pg.path_list);
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
			del_pel_list(&fent->pg.path_list);
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
	del_pel_list(&pg_to.path_list);
	del_pel_list(&pg_from.path_list);
	return 0;
}

static int handle_symlink(const char *from, const char *to,
			  struct file_info *info)
{
	struct sym_entry *sym;

	sym = malloc(sizeof *sym);
	if(!sym) {
		perror("malloc");
		return -1;
	}
	sym->from = strdup(from);
	if(!sym->from) {
		perror("strdup");
		return -1;
	}
	sym->to = strdup(to);
	if(!sym->to) {
		perror("strdup");
		return -1;
	}
	list_add(&sym->list, &info->sym_list);
	return 0;
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

		/* TODO: Do we need this? I think this should only apply to
		 * temporary files.
		 */
/*		delete_name_file(u->tupid);*/
		del_entry(u);
	}
}
