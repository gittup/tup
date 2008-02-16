#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "access_event.h"
#include "debug.h"
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
static int create_file(char *filename);
static int mkdirhier(char *filename);
static int __mkdirhier(char *dirname);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static LIST_HEAD(rename_list);

int handle_file(const struct access_event *event)
{
	struct file_entry *fent;
	int rc = 0;

	DEBUGP("tup-server[%i]: Received file '%s' in mode %i\n",
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

	list_for_each_entry(w, &write_list, list) {
		DEBUGP("Write deps for file: %s.\n", w->file);
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

	if(strcmp(file, depends_on) == 0) {
		DEBUGP("File '%s' dependent on itself - ignoring.\n", file);
		/* Ignore dependencies of a file on itself (ie: the file is
		 * opened for both read and write).
		 */
		return 0;
	}

	if(snprintf(tupd, sizeof(tupd), "%s.tupd/%s", depends_on, file) >=
	   (signed)sizeof(tupd)) {
		fprintf(stderr, "Filename (%s.tupd/%s) too long\n",
			depends_on, file);
		return -1;
	}

	create_file(tupd);
	return 0;
}

static int create_file(char *filename)
{
	struct stat buf;
	int rc;
	int fd;

	DEBUGP("Create file: '%s'\n", filename);

	/* Quick check to see if the file already exists. */
	rc = stat(filename, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", filename);
			return -1;
		}
	}

	if(mkdirhier(filename) < 0) {
		return -1;
	}

	fd = creat(filename, 0666);
	if(fd < 0) {
		perror("creat");
		return -1;
	}
	close(fd);
	return 0;
}

static int mkdirhier(char *filename)
{
	int rc = 0;
	char *slash;
	char *tmp;

	tmp = filename;
	do {
		slash = strchr(tmp, '/');
		if(slash == NULL)
			break;

		tmp = slash+1;
		*slash = 0;
		rc = __mkdirhier(filename);
		*slash = '/';
	} while(rc == 0);

	return rc;
}

static int __mkdirhier(char *dirname)
{
	int rc;
	struct stat buf;

	DEBUGP("Mkdirhier: '%s'\n", dirname);
	rc = stat(dirname, &buf);
	if(rc == 0) {
		if(!S_ISDIR(buf.st_mode)) {
			fprintf(stderr, "Error: '%s' is not a directory.\n",
				dirname);
			return -1;
		}
	} else {
		if(mkdir(dirname, 0777) < 0) {
			perror("mkdir");
			return -1;
		}
	}
	return 0;
}
