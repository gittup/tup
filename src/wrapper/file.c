#include "file.h"
#include "tup/fileio.h"
#include "tup/access_event.h"
#include "tup/debug.h"
#include "tup/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

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

	DEBUGP("received tupid '%.*s' in mode %i\n",
	       sizeof(event->tupid), event->tupid, event->at);

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

int write_files(void)
{
	struct file_entry *w;
	struct file_entry *r;

	list_for_each_entry(w, &write_list, list) {
		DEBUGP("Write deps for tupid: %.*s.\n",
		       sizeof(w->tupid), w->tupid);
		if(recreate_name_file(w->tupid) < 0)
			return -1;
		list_for_each_entry(r, &read_list, list) {
			if(memcmp(w->tupid, r->tupid, sizeof(w->tupid)) == 0) {
				DEBUGP("tupid '%.*s' dependent on itself - "
				       "ignoring.\n",
				       sizeof(w->tupid), w->tupid);
				continue;
			}
			if(create_secondary_link(r->tupid, w->tupid) < 0)
				return -1;
		}
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

	memcpy(fent->tupid, event->tupid, sizeof(fent->tupid));
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
		if(memcmp(fent->tupid, from->tupid, sizeof(fent->tupid)) == 0) {
			memcpy(fent->tupid, event->tupid, sizeof(fent->tupid));
		}
	}
	list_for_each_entry(fent, &read_list, list) {
		if(memcmp(fent->tupid, from->tupid, sizeof(fent->tupid)) == 0) {
			memcpy(fent->tupid, event->tupid, sizeof(fent->tupid));
		}
	}
	return 0;
}
