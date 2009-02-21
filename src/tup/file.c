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
	int pid;
	struct list_head list;
};

static struct file_entry *new_entry(const struct access_event *event,
				    const char *filename);
static void del_entry(struct file_entry *fent);
static int handle_rename_to(int pid, const char *filename,
			    struct file_info *info);
static int __handle_rename_to(struct file_entry *from, const char *filename,
			      struct file_info *info);
static void check_unlink_list(const char *filename, struct list_head *u_list);
static void handle_unlink(struct file_info *info);

int init_file_info(struct file_info *info)
{
	INIT_LIST_HEAD(&info->read_list);
	INIT_LIST_HEAD(&info->write_list);
	INIT_LIST_HEAD(&info->rename_list);
	INIT_LIST_HEAD(&info->unlink_list);
	return 0;
}

int handle_file(const struct access_event *event, const char *filename,
		struct file_info *info)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("received file '%s' in mode %i\n", filename, event->at);

	if(event->at == ACCESS_RENAME_TO) {
		check_unlink_list(filename, &info->unlink_list);
		return handle_rename_to(event->pid, filename, info);
	}

	fent = new_entry(event, filename);
	if(!fent) {
		return -1;
	}

	switch(event->at) {
		case ACCESS_READ:
			list_add(&fent->list, &info->read_list);
			break;
		case ACCESS_WRITE:
			check_unlink_list(filename, &info->unlink_list);
			list_add(&fent->list, &info->write_list);
			break;
		case ACCESS_RENAME_FROM:
			list_add(&fent->list, &info->rename_list);
			break;
		case ACCESS_UNLINK:
			list_add(&fent->list, &info->unlink_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", event->at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(tupid_t cmdid, tupid_t old_cmdid, const char *debug_name,
		struct file_info *info)
{
	struct file_entry *w;
	struct file_entry *r;
	struct db_node dbn;
	struct tupid_list *tl;
	int output_bork = 0;
	LIST_HEAD(old_output_list);
	LIST_HEAD(old_input_list);

	handle_unlink(info);

	if(tup_db_get_dest_links(old_cmdid, &old_output_list) < 0)
		return -1;
	if(tup_db_get_src_links(old_cmdid, &old_input_list) < 0)
		return -1;

	while(!list_empty(&info->write_list)) {
		struct file_entry *tmp;
		w = list_entry(info->write_list.next, struct file_entry, list);

		if(get_dbn(w->filename, &dbn) < 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", w->filename, debug_name);
			return -1;
		}

		if(tup_db_create_link(cmdid, dbn.tupid) < 0)
			return -1;
		list_for_each_entry_safe(r, tmp, &info->read_list, list) {
			if(strcmp(w->filename, r->filename) == 0)
				del_entry(r);
		}
		list_for_each_entry(tl, &old_output_list, list) {
			if(tl->tupid == dbn.tupid) {
				/* Ok to do list_del without _safe because we
				 * break out of the loop (links are unique, so
				 * there is only going to be one matching tupid
				 * in the old_output_list).
				 */
				list_del(&tl->list);
				free(tl);
				break;
			}
		}

		del_entry(w);
	}
	list_for_each_entry(tl, &old_output_list, list) {
		fprintf(stderr, "Error: Tupid %lli was supposed to be written to by command %lli, but it wasn't.\n", tl->tupid, old_cmdid);
		output_bork = 1;
	}
	if(output_bork)
		return -1;

	while(!list_empty(&info->read_list)) {
		int rc;
		r = list_entry(info->read_list.next, struct file_entry, list);
		if(get_dbn(r->filename, &dbn) < 0) {
			fprintf(stderr, "tup error: File '%s' was read from, but is not in .tup/db. It was read from command '%s' - not sure why it isn't there.\n", r->filename, debug_name);
			return -1;
		}
		/* Root nodes are always cool */
		rc = tup_db_is_root_node(dbn.tupid);
		if(rc < 0)
			return -1;
		if(rc == 1)
			goto link_cool;
		/* Non-root nodes that are specified as input links in the
		 * Tupfile are also cool.
		 */
		list_for_each_entry(tl, &old_input_list, list) {
			if(tl->tupid == dbn.tupid)
				goto link_cool;
		}
		/* Non-coolness is not allowed. */
		fprintf(stderr, "tup error: File '%s' was read from, is generated from another command, and was not specified as an input link for command '%s'. You should add this file as an input, since it is possible this could randomly break in the future.\n", r->filename, debug_name);
		return -1;
link_cool:
		if(tup_db_create_link(dbn.tupid, cmdid) < 0)
			return -1;
		del_entry(r);
	}
	return 0;
}

static struct file_entry *new_entry(const struct access_event *event,
				    const char *filename)
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
	fent->pid = event->pid;
	return fent;
}

static void del_entry(struct file_entry *fent)
{
	list_del(&fent->list);
	free(fent->filename);
	free(fent);
}

static int handle_rename_to(int pid, const char *filename,
			    struct file_info *info)
{
	struct file_entry *from;

	list_for_each_entry(from, &info->rename_list, list) {
		if(from->pid == pid) {
			return __handle_rename_to(from, filename, info);
		}
	}
	fprintf(stderr, "Error: ACCESS_RENAME_TO event corresponding to pid %i "
		"not found in rename_list.\n", pid);
	return -1;
}

static int __handle_rename_to(struct file_entry *from, const char *filename,
			      struct file_info *info)
{
	struct file_entry *fent;

	list_for_each_entry(fent, &info->write_list, list) {
		if(strcmp(fent->filename, from->filename) == 0) {
			free(fent->filename);
			fent->filename = strdup(filename);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	list_for_each_entry(fent, &info->read_list, list) {
		if(strcmp(fent->filename, from->filename) == 0) {
			free(fent->filename);
			fent->filename = strdup(filename);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	list_move(&from->list, &info->unlink_list);
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
