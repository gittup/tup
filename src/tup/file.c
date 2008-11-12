#include "file.h"
#include "tup/access_event.h"
#include "tup/debug.h"
#include "tup/list.h"
#include "tup/db.h"
#include <stdio.h>
#include <stdlib.h>

struct file_entry {
	tupid_t tupid;
	int pid;
	struct list_head list;
};

static struct file_entry *new_entry(const struct access_event *event);
static int handle_rename_to(const struct access_event *event);
static int __handle_rename_to(struct file_entry *from,
			      const struct access_event *event);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static LIST_HEAD(rename_list);

int handle_file(const struct access_event *event)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("received tupid '%lli' in mode %i\n", event->tupid, event->at);

	if(event->at == ACCESS_RENAME_TO) {
		return handle_rename_to(event);
	}

	fent = new_entry(event);
	if(!fent) {
		return -1;
	}

	switch(event->at) {
		case ACCESS_READ:
			list_add(&fent->list, &read_list);
			break;
		case ACCESS_WRITE:
			list_add(&fent->list, &write_list);
			break;
		case ACCESS_RENAME_FROM:
			list_add(&fent->list, &rename_list);
			break;
		default:
			fprintf(stderr, "Invalid event type: %i\n", event->at);
			rc = -1;
			break;
	}

	return rc;
}

int write_files(tupid_t cmdid)
{
	struct file_entry *w;
	struct file_entry *r;

	list_for_each_entry(w, &write_list, list) {
		struct file_entry *tmp;

		if(tup_db_create_link(cmdid, w->tupid) < 0)
			return -1;
		list_for_each_entry_safe(r, tmp, &read_list, list) {
			if(w->tupid == r->tupid)
				list_del(&r->list);
		}
	}

	list_for_each_entry(r, &read_list, list) {
		if(tup_db_create_link(r->tupid, cmdid) < 0)
			return -1;
	}
	return 0;
}

static struct file_entry *new_entry(const struct access_event *event)
{
	struct file_entry *fent;

	fent = malloc(sizeof *fent);
	if(!fent) {
		perror("malloc");
		return NULL;
	}

	fent->tupid = event->tupid;
	fent->pid = event->pid;
	return fent;
}

static int handle_rename_to(const struct access_event *event)
{
	struct file_entry *from;

	list_for_each_entry(from, &rename_list, list) {
		if(from->pid == event->pid) {
			return __handle_rename_to(from, event);
		}
	}
	fprintf(stderr, "Error: ACCESS_RENAME_TO event corresponding to pid %i "
		"not found in rename_list.\n", event->pid);
	return -1;
}

static int __handle_rename_to(struct file_entry *from,
			      const struct access_event *event)
{
	struct file_entry *fent;

	list_del(&from->list);

	list_for_each_entry(fent, &write_list, list) {
		if(fent->tupid == from->tupid) {
			fent->tupid = event->tupid;
		}
	}
	list_for_each_entry(fent, &read_list, list) {
		if(fent->tupid == from->tupid) {
			fent->tupid = event->tupid;
		}
	}
	return 0;
}
