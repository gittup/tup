#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "list.h"
#include "db.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>

struct file_entry {
	tupid_t tupid;
	int pid;
	struct list_head list;
};

static struct file_entry *new_entry(const struct access_event *event,
				    tupid_t tupid);
static void del_entry(struct file_entry *fent);
static int handle_rename_to(int pid, tupid_t tupid);
static int __handle_rename_to(struct file_entry *from, tupid_t tupid);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static LIST_HEAD(rename_list);

int handle_file(const struct access_event *event, const char *filename)
{
	struct file_entry *fent;
	int rc = 0;
	tupid_t tupid;

	tupid = create_name_file(filename);
	if(tupid < 0)
		return -1;
	DEBUGP("received tupid '%lli' in mode %i\n", tupid, event->at);

	if(event->at == ACCESS_RENAME_TO) {
		return handle_rename_to(event->pid, tupid);
	}

	fent = new_entry(event, tupid);
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

	while(!list_empty(&write_list)) {
		struct file_entry *tmp;
		w = list_entry(write_list.next, struct file_entry, list);

		if(tup_db_create_link(cmdid, w->tupid) < 0)
			return -1;
		list_for_each_entry_safe(r, tmp, &read_list, list) {
			if(w->tupid == r->tupid)
				del_entry(r);
		}

		del_entry(w);
	}

	while(!list_empty(&read_list)) {
		r = list_entry(read_list.next, struct file_entry, list);
		if(tup_db_create_link(r->tupid, cmdid) < 0)
			return -1;
		del_entry(r);
	}
	return 0;
}

static struct file_entry *new_entry(const struct access_event *event,
				    tupid_t tupid)
{
	struct file_entry *fent;

	fent = malloc(sizeof *fent);
	if(!fent) {
		perror("malloc");
		return NULL;
	}

	fent->tupid = tupid;
	fent->pid = event->pid;
	return fent;
}

static void del_entry(struct file_entry *fent)
{
	list_del(&fent->list);
	free(fent);
}

static int handle_rename_to(int pid, tupid_t tupid)
{
	struct file_entry *from;

	list_for_each_entry(from, &rename_list, list) {
		if(from->pid == pid) {
			return __handle_rename_to(from, tupid);
		}
	}
	fprintf(stderr, "Error: ACCESS_RENAME_TO event corresponding to pid %i "
		"not found in rename_list.\n", pid);
	return -1;
}

static int __handle_rename_to(struct file_entry *from, tupid_t tupid)
{
	struct file_entry *fent;

	list_for_each_entry(fent, &write_list, list) {
		if(fent->tupid == from->tupid) {
			fent->tupid = tupid;
		}
	}
	list_for_each_entry(fent, &read_list, list) {
		if(fent->tupid == from->tupid) {
			fent->tupid = tupid;
		}
	}
	del_entry(from);
	return 0;
}
