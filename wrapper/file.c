#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* TODO */
#include "../ldpreload/access_event.h"
#include "list.h"

struct file_entry {
	char *file;
	struct list_head list;
};

static struct file_entry *new_entry(const char *file);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);

void handle_file(const struct access_event *event)
{
	struct file_entry *fent;

	fprintf(stderr, "MARF[%i]: File '%s' in mode %i\n",
		getpid(), event->filename, event->rw);

	fent = new_entry(event->filename);
	if(!fent) {
		return;
	}

	if(event->rw == 0) {
		list_add(&fent->list, &read_list);
	} else {
		list_add(&fent->list, &write_list);
	}
}

void write_files(void)
{
	struct file_entry *w;
	struct file_entry *r;

	fprintf(stderr, "Write files.\n");
	list_for_each_entry(w, &write_list, list) {
	fprintf(stderr, "Write files: %s.\n", w->file);
		list_for_each_entry(r, &read_list, list) {
			fprintf(stderr, "Write out that %s depends on %s\n", w->file, r->file);
		}
	}
}

static struct file_entry *new_entry(const char *file)
{
	struct file_entry *tmp;

	tmp = malloc(sizeof *tmp);
	if(!tmp) {
		perror("malloc");
		return NULL;
	}

	tmp->file = malloc(strlen(file) + 1);
	if(!tmp->file) {
		perror("malloc");
		free(tmp);
		return NULL;
	}
	strcpy(tmp->file, file);
	return tmp;
}
