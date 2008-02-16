#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "access_event.h"
#include "list.h"

struct file_entry {
	char *file;
	int pid;
	struct list_head list;
};

static struct file_entry *new_entry(const struct access_event *event);
static int handle_rename_to(const struct access_event *event);
static int __handle_rename_to(struct file_entry *rfrom,
			      const struct access_event *event);
static int write_dep(const char *file, const char *depends_on);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static LIST_HEAD(rename_list);

int handle_file(const struct access_event *event)
{
	struct file_entry *fent;
	int rc = 0;

	fprintf(stderr, "tup-server[%i]: Received file '%s' in mode %i\n",
		getpid(), event->file, event->at);

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

	fprintf(stderr, "Write files.\n");

	list_for_each_entry(w, &write_list, list) {
		fprintf(stderr, "Write files: %s.\n", w->file);
		list_for_each_entry(r, &read_list, list) {
			if(write_dep(w->file, r->file) < 0)
				return -1;
		}
	}
	return 0;
}

static struct file_entry *new_entry(const struct access_event *event)
{
	struct file_entry *tmp;

	tmp = malloc(sizeof *tmp);
	if(!tmp) {
		perror("malloc");
		return NULL;
	}

	tmp->file = malloc(strlen(event->file) + 1);
	if(!tmp->file) {
		perror("malloc");
		free(tmp);
		return NULL;
	}
	strcpy(tmp->file, event->file);
	tmp->pid = event->pid;
	return tmp;
}

static int handle_rename_to(const struct access_event *event)
{
	struct file_entry *rfrom;

	list_for_each_entry(rfrom, &rename_list, list) {
		if(rfrom->pid == event->pid) {
			return __handle_rename_to(rfrom, event);
		}
	}
	fprintf(stderr, "Error: ACCESS_RENAME_TO event corresponding to pid %i "
		"not found in rename_list.\n", event->pid);
	return -1;
}

static int __handle_rename_to(struct file_entry *rfrom,
			      const struct access_event *event)
{
	struct file_entry *fent;

	list_del(&rfrom->list);

	list_for_each_entry(fent, &write_list, list) {
		if(strcmp(fent->file, rfrom->file) == 0) {
			free(fent->file);
			fent->file = malloc(strlen(event->file) + 1);
			if(!fent->file)
				return -1;
			strcpy(fent->file, event->file);
		}
	}
	list_for_each_entry(fent, &read_list, list) {
		if(strcmp(fent->file, rfrom->file) == 0) {
			free(fent->file);
			fent->file = malloc(strlen(event->file) + 1);
			if(!fent->file)
				return -1;
			strcpy(fent->file, event->file);
		}
	}
	return 0;
}

static int write_dep(const char *file, const char *depends_on)
{
	/* In make this would be written as:
	 *  file: depends_on
	 * eg:
	 *  foo.o: foo.c
	 */
	static char tupd[MAXPATHLEN];
	static char linkdest[MAXPATHLEN];
	struct stat buf;
	int rc;

	if(strcmp(file, depends_on) == 0) {
		/* Ignore dependencies of a file on itself (ie: the file is
		 * opened for both read and write.
		 */
		return 0;
	}

	/* TODO: Proper string handling (strlcpy/strlcat?) */
	strcpy(linkdest, "../");
	strcat(linkdest, file);
	strcpy(tupd, depends_on);
	strcat(tupd, ".tupd");
	rc = stat(tupd, &buf);
	if(rc == 0) {
		if(!S_ISDIR(buf.st_mode)) {
			fprintf(stderr, "Error: '%s' is not a directory.\n",
				tupd);
			return -1;
		}
	} else {
		if(mkdir(tupd, 0777) < 0) {
			perror("mkdir");
			return -1;
		}
	}
	strcat(tupd, "/");
	strcat(tupd, file);
	fprintf(stderr, "Symlink: '%s' -> '%s'\n", tupd, file);
	unlink(tupd);
	if(symlink(linkdest, tupd) < 0) {
		perror("symlink");
	}
	return 0;
}
