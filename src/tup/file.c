#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "list.h"
#include "db.h"
#include "fileio.h"
#include "config.h" /* TODO */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file_entry {
	char *filename;
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
static void check_unlink_list(const char *filename, struct list_head *u_list);
static void handle_unlink(struct file_info *info);

int init_file_info(struct file_info *info)
{
	INIT_LIST_HEAD(&info->read_list);
	INIT_LIST_HEAD(&info->write_list);
	INIT_LIST_HEAD(&info->unlink_list);
	INIT_LIST_HEAD(&info->tupid_list);
	INIT_LIST_HEAD(&info->sym_list);
	return 0;
}

int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("received file '%s' in mode %i\n", filename, at);

	if(at == ACCESS_RENAME) {
		check_unlink_list(file2, &info->unlink_list);
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
			check_unlink_list(filename, &info->unlink_list);
			list_add(&fent->list, &info->write_list);
			break;
		case ACCESS_UNLINK:
			list_add(&fent->list, &info->unlink_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", at);
			rc = -1;
			break;
	}

	return rc;
}

int handle_tupid(tupid_t tupid, struct file_info *info)
{
	struct id_entry *ide;

	ide = malloc(sizeof *ide);
	if(!ide) {
		perror("malloc");
		return -1;
	}
	ide->tupid = tupid;
	list_add(&ide->list, &info->tupid_list);
	return 0;
}

int write_files(tupid_t cmdid, tupid_t old_cmdid, tupid_t dt,
		const char *debug_name, struct file_info *info)
{
	struct file_entry *w;
	struct file_entry *r;
	struct db_node dbn;
	struct id_entry *ide;
	struct half_entry *he;
	int output_bork = 0;
	LIST_HEAD(old_output_list);

	handle_unlink(info);

	if(tup_db_get_dest_links(old_cmdid, &old_output_list) < 0)
		return -1;

	while(!list_empty(&info->write_list)) {
		struct file_entry *tmp;
		w = list_entry(info->write_list.next, struct file_entry, list);

		if(tup_db_select_dbn(dt, w->filename, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", w->filename, debug_name);
			return -1;
		}

		if(tup_db_create_link(cmdid, dbn.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
		list_for_each_entry_safe(r, tmp, &info->read_list, list) {
			if(strcmp(w->filename, r->filename) == 0)
				del_entry(r);
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
				break;
			}
		}

		del_entry(w);
	}

	while(!list_empty(&info->sym_list)) {
		struct sym_entry *sym;
		struct db_node dbn_link;

		sym = list_entry(info->sym_list.next, struct sym_entry, list);

		if(tup_db_select_dbn(dt, sym->to, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			fprintf(stderr, "tup error: File '%s' was written as a symlink, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", sym->to, debug_name);
			return -1;
		}

		/* TODO: curdt? */
		/* TODO: Don't need symlist? */
		/* TODO: symlinks to non-existent files - create new node? */
		if(get_dbn_dt(dt, sym->from, &dbn_link, NULL) < 0) {
			fprintf(stderr, "tup error: Attempting to create a symlink to file '%s' from dir %lli, but the destination file doesn't exist in .tup/db. Maybe you should link to an existing file?\n", sym->from, dt);
			return -1;
		}

		if(tup_db_create_link(cmdid, dbn.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
		if(tup_db_set_sym(dbn.tupid, dbn_link.tupid) < 0)
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
				break;
			}
		}

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
		const char *path;
		tupid_t curdt;
		LIST_HEAD(symlist);

		r = list_entry(info->read_list.next, struct file_entry, list);

		path = r->filename;
		curdt = dt;
		if(r->filename[0] == '/') {
			int ttl = get_tup_top_len();
			if(strncmp(r->filename, get_tup_top(), ttl) != 0 ||
			   r->filename[ttl] != '/') {
/*				fprintf(stderr, "Error: The path '%s' is not in the tup hierarchy.\n", r->filename);*/
				goto out_skip;
			}
			path += ttl + 1;
			curdt = DOT_DT;
		}

		if(get_dbn_dt(curdt, path, &dbn, &symlist) < 0) {
			fprintf(stderr, "tup error: File '%s' was read from, but is not in .tup/db. It was read from command '%s' - not sure why it isn't there.\n", r->filename, debug_name);
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
out_skip:
		del_entry(r);
	}

	while(!list_empty(&info->tupid_list)) {
		ide = list_entry(info->tupid_list.next, struct id_entry, list);
		if(tup_db_create_link(ide->tupid, cmdid, TUP_LINK_NORMAL) < 0)
			return -1;
		list_del(&ide->list);
		free(ide);
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
	return fent;
}

static void del_entry(struct file_entry *fent)
{
	list_del(&fent->list);
	free(fent->filename);
	free(fent);
}

static int handle_rename(const char *from, const char *to,
			 struct file_info *info)
{
	struct file_entry *fent;

	list_for_each_entry(fent, &info->write_list, list) {
		if(strcmp(fent->filename, from) == 0) {
			free(fent->filename);
			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	list_for_each_entry(fent, &info->read_list, list) {
		if(strcmp(fent->filename, from) == 0) {
			free(fent->filename);
			fent->filename = strdup(to);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
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

static void check_unlink_list(const char *filename, struct list_head *u_list)
{
	struct file_entry *fent, *tmp;

	list_for_each_entry_safe(fent, tmp, u_list, list) {
		if(strcmp(fent->filename, filename) == 0) {
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
			if(strcmp(fent->filename, u->filename) == 0) {
				del_entry(fent);
			}
		}
		list_for_each_entry_safe(fent, tmp, &info->read_list, list) {
			if(strcmp(fent->filename, u->filename) == 0) {
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
