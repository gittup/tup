#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "list.h"
#include "db.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int write_files(tupid_t cmdid, tupid_t old_cmdid, tupid_t dt,
		const char *debug_name, struct file_info *info)
{
	struct file_entry *w;
	struct file_entry *r;
	struct file_entry *g;
	struct file_entry *tmp;
	struct db_node dbn;
	struct id_entry *ide;
	struct half_entry *he;
	int output_bork = 0;
	LIST_HEAD(old_output_list);

	handle_unlink(info);

	if(tup_db_get_dest_links(old_cmdid, &old_output_list) < 0)
		return -1;

	while(!list_empty(&info->write_list)) {
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

		/* TODO: need symlist here? What if a command writes to a
		 * full-path that uses a symlink?
		 */
		if(get_dbn_dt_pg(dt, &w->pg, &dbn, NULL) <= 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", w->filename, debug_name);
			return -1;
		}
		if(dbn.tupid < 0) {
			fprintf(stderr, "[31mtup internal error: dbn.tupid < 0 in write_files() write_list?[0m\n");
			return -1;
		}
		if(dbn.dt != dt) {
			fprintf(stderr, "tup error: File '%s' was written to by command '%s', but the command should only write files to directory %lli\n", w->filename, debug_name, dt);
			goto write_fail;
		}

		if(tup_db_create_link(cmdid, dbn.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
		list_for_each_entry(ide, &old_output_list, list) {
			if(ide->tupid == dbn.tupid) {
				/* Ok to do list_del without _safe because we
				 * break out of the loop (links are unique, so
				 * there is only going to be one matching tupid
				 * in the old_output_list).
				 */
				list_del(&ide->list);
				free(ide);
				goto write_ok;
			}
		}

		/* Oops, wrote to a file that we're not allowed to. Make sure
		 * we re-run whatever command was supposed to write to that
		 * file.
		 */
		fprintf(stderr, "tup error: File '%s' was written to by command '%s', but this was not specified in the Tupfile in dir %lli.\n", w->filename, debug_name, dt);
write_fail:
		output_bork = 1;
		tup_db_modify_cmds_by_output(dbn.tupid);

write_ok:
		del_entry(w);
	}

	while(!list_empty(&info->sym_list)) {
		struct sym_entry *sym;
		struct db_node dbn_link;
		tupid_t dbn_tupid;

		sym = list_entry(info->sym_list.next, struct sym_entry, list);

		if(tup_db_select_dbn(dt, sym->to, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			fprintf(stderr, "tup error: File '%s' was written as a symlink, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", sym->to, debug_name);
			return -1;
		}

		/* TODO: Don't need symlist? */
		/* TODO: symlinks to non-existent files - create new node? */
		dbn_tupid = get_dbn_dt(dt, sym->from, &dbn_link, NULL);
		if(dbn_tupid < 0) {
			fprintf(stderr, "tup error: Attempting to create a symlink to file '%s' from dir %lli, but the destination file doesn't exist in .tup/db. Maybe you should link to an existing file?\n", sym->from, dt);
			return -1;
		}
		/* Skip files outside of .tup */
		if(dbn_tupid == 0)
			goto skip_sym;

		if(tup_db_create_link(cmdid, dbn.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
		if(tup_db_set_sym(dbn.tupid, dbn_link.tupid) < 0)
			return -1;

		list_for_each_entry_safe(g, tmp, &info->ghost_list, list) {
			/* Use strcmp instead of pg_eq because we don't have
			 * the pgs for sym_entries. Also this should only
			 * happen when 'ln' does a stat() before it does a
			 * symlink().
			 */
			if(strcmp(sym->to, g->filename) == 0)
				del_entry(g);
		}

		list_for_each_entry(ide, &old_output_list, list) {
			if(ide->tupid == dbn.tupid) {
				/* Ok to do list_del without _safe because we
				 * break out of the loop (links are unique, so
				 * there is only going to be one matching tupid
				 * in the old_output_list).
				 */
				list_del(&ide->list);
				free(ide);
				goto skip_sym;
			}
		}

		/* Similar to the write case, make sure we were supposed to
		 * actually create this file.
		 */
		fprintf(stderr, "tup error: File '%s' was made as a symlink by command '%s', but this was not specified in the Tupfile in dir %lli.\n", sym->to, debug_name, dt);
		output_bork = 1;
		tup_db_modify_cmds_by_output(dbn.tupid);

skip_sym:
		list_del(&sym->list);
		free(sym->from);
		free(sym->to);
		free(sym);
	}

	list_for_each_entry(ide, &old_output_list, list) {
		fprintf(stderr, "Error: Tupid %lli was supposed to be written to by command %lli, but it wasn't.\n", ide->tupid, old_cmdid);
		output_bork = 1;
	}
	if(output_bork)
		return -1;

	while(!list_empty(&info->read_list)) {
		tupid_t dbn_tupid;
		LIST_HEAD(symlist);

		r = list_entry(info->read_list.next, struct file_entry, list);

		dbn_tupid = get_dbn_dt_pg(dt, &r->pg, &dbn, &symlist);
		if(dbn_tupid < 0) {
			fprintf(stderr, "tup error: File '%s' was read from, but is not in .tup/db. It was read from command '%s' - not sure why it isn't there.\n", r->filename, debug_name);
			return -1;
		}
		/* Skip files outside of .tup */
		if(dbn_tupid == 0)
			goto skip_read;
		while(!list_empty(&symlist)) {
			he = list_entry(symlist.next, struct half_entry, list);
			if(tup_db_create_link(he->tupid, cmdid, TUP_LINK_NORMAL) < 0)
				return -1;
			list_del(&he->list);
			free(he);
		}
		if(tup_db_create_link(dbn.tupid, cmdid, TUP_LINK_NORMAL) < 0)
			return -1;

skip_read:
		del_entry(r);
	}

	while(!list_empty(&info->var_list)) {
		r = list_entry(info->var_list.next, struct file_entry, list);
		if(tup_db_select_dbn(VAR_DT, r->filename, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			fprintf(stderr, "Error: Unable to find tupid for variable named '%s'\n", r->filename);
			return -1;
		}
		if(tup_db_create_link(dbn.tupid, cmdid, TUP_LINK_NORMAL) < 0)
			return -1;
		del_entry(r);
	}

	while(!list_empty(&info->ghost_list)) {
		const char *file;
		tupid_t newdt;
		LIST_HEAD(symlist);

		g = list_entry(info->ghost_list.next, struct file_entry, list);
		newdt = find_dir_tupid_dt_pg(dt, &g->pg, &file, &symlist, 1);
		if(newdt < 0) {
			fprintf(stderr, "Error finding dir for '%s' relative to dir %lli\n", g->filename, dt);
			return 0;
		}
		if(newdt == 0)
			goto skip_ghost;

		if(file) {
			if(tup_db_select_dbn(newdt, file, &dbn) < 0)
				return -1;
			if(dbn.tupid < 0) {
				dbn.tupid = tup_db_node_insert(newdt, file, -1, TUP_NODE_GHOST);
				if(dbn.tupid < 0)
					return -1;
			} else {
				if(sym_follow(&dbn, &symlist) < 0)
					return -1;
			}

			while(!list_empty(&symlist)) {
				he = list_entry(symlist.next, struct half_entry, list);
				if(tup_db_create_link(he->tupid, cmdid, TUP_LINK_NORMAL) < 0)
					return -1;
				list_del(&he->list);
				free(he);
			}
			if(tup_db_create_link(dbn.tupid, cmdid, TUP_LINK_NORMAL) < 0)
				return -1;
		} else {
			fprintf(stderr, "[31mtup internal error: processing the ghost list didn't get a final file pointer in find_dir_tupid_dt_pg()[0m\n");
			return -1;
		}

skip_ghost:
		del_entry(g);
	}
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
