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
static int handle_rename_to(int pid, const char *filename);
static int __handle_rename_to(struct file_entry *from, const char *filename);
static void check_unlink_list(const char *filename);
static void handle_unlink(void);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static LIST_HEAD(rename_list);
static LIST_HEAD(unlink_list);

int handle_file(const struct access_event *event, const char *filename)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("received file '%s' in mode %i\n", filename, event->at);

	if(event->at == ACCESS_RENAME_TO) {
		check_unlink_list(filename);
		return handle_rename_to(event->pid, filename);
	}

	fent = new_entry(event, filename);
	if(!fent) {
		return -1;
	}

	switch(event->at) {
		case ACCESS_READ:
			list_add(&fent->list, &read_list);
			break;
		case ACCESS_WRITE:
			check_unlink_list(filename);
			list_add(&fent->list, &write_list);
			break;
		case ACCESS_RENAME_FROM:
			list_add(&fent->list, &rename_list);
			break;
		case ACCESS_UNLINK:
			list_add(&fent->list, &unlink_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", event->at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(tupid_t cmdid, const char *debug_name)
{
	struct file_entry *w;
	struct file_entry *r;
	struct db_node dbn;

	handle_unlink();

	while(!list_empty(&write_list)) {
		struct file_entry *tmp;
		w = list_entry(write_list.next, struct file_entry, list);

		if(get_dbn(w->filename, &dbn) < 0) {
			fprintf(stderr, "tup error: File '%s' was written to, but is not in .tup/db. You probably should specify it as an output for command '%s'\n", w->filename, debug_name);
			return -1;
		}

		if(tup_db_create_link(cmdid, dbn.tupid) < 0)
			return -1;
		list_for_each_entry_safe(r, tmp, &read_list, list) {
			if(strcmp(w->filename, r->filename) == 0)
				del_entry(r);
		}

		del_entry(w);
	}

	while(!list_empty(&read_list)) {
		r = list_entry(read_list.next, struct file_entry, list);
		if(get_dbn(r->filename, &dbn) < 0) {
			fprintf(stderr, "tup error: File '%s' was read from, but is not in .tup/db. It was read from command '%s' - not sure why it isn't there.\n", r->filename, debug_name);
			return -1;
		}
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

static int handle_rename_to(int pid, const char *filename)
{
	struct file_entry *from;

	list_for_each_entry(from, &rename_list, list) {
		if(from->pid == pid) {
			return __handle_rename_to(from, filename);
		}
	}
	fprintf(stderr, "Error: ACCESS_RENAME_TO event corresponding to pid %i "
		"not found in rename_list.\n", pid);
	return -1;
}

static int __handle_rename_to(struct file_entry *from, const char *filename)
{
	struct file_entry *fent;

	list_for_each_entry(fent, &write_list, list) {
		if(strcmp(fent->filename, from->filename) == 0) {
			free(fent->filename);
			fent->filename = strdup(filename);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	list_for_each_entry(fent, &read_list, list) {
		if(strcmp(fent->filename, from->filename) == 0) {
			free(fent->filename);
			fent->filename = strdup(filename);
			if(!fent->filename) {
				perror("strdup");
				return -1;
			}
		}
	}
	list_move(&from->list, &unlink_list);
	return 0;
}

static void check_unlink_list(const char *filename)
{
	struct file_entry *fent, *tmp;

	list_for_each_entry_safe(fent, tmp, &unlink_list, list) {
		if(strcmp(fent->filename, filename) == 0) {
			del_entry(fent);
		}
	}
}

static void handle_unlink(void)
{
	struct file_entry *u, *fent, *tmp;

	while(!list_empty(&unlink_list)) {
		u = list_entry(unlink_list.next, struct file_entry, list);

		list_for_each_entry_safe(fent, tmp, &write_list, list) {
			if(strcmp(fent->filename, u->filename) == 0) {
				del_entry(fent);
			}
		}
		list_for_each_entry_safe(fent, tmp, &read_list, list) {
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
