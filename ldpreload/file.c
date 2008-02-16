#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "file.h"
#include "list.h"

struct file_entry {
	char *file;
	struct list_head list;
};

static void write_files(void);
static struct file_entry *new_entry(const char *file);

static LIST_HEAD(read_list);
static LIST_HEAD(write_list);
static struct sockaddr_un server;
static int server_sd = -1;

void handle_file(const char *file, int rw, const char *func)
{
	struct file_entry *fent;

	if(strncmp(file, "/tmp/", 5) == 0) {
		return;
	}
	fprintf(stderr, "MARF[%i]: File '%s' in mode %i from %s\n", getpid(), file, rw, func);

	fent = new_entry(file);
	if(!fent) {
		return;
	}

	if(rw == 0) {
		list_add(&fent->list, &read_list);
		{
			struct file_entry *tmp;
			fprintf(stderr, "LIst:\n");
			list_for_each_entry(tmp, &read_list, list) {
				fprintf(stderr, " File: %s\n", tmp->file);
			}
		}
	} else {
		list_add(&fent->list, &write_list);
	}
}

int start_server(void)
{
	server_sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if(server_sd < 0) {
		perror("socket");
		return -1;
	}
	server.sun_family = AF_UNIX;
	snprintf(server.sun_path, sizeof(server.sun_path)-1, "/tmp/tup-%i",
		 getpid());
	server.sun_path[sizeof(server.sun_path)-1] = 0;

	if(bind(server_sd, (void*)&server, sizeof(server)) < 0) {
		perror("bind");
		close(server_sd);
		return -1;
	}

	return 0;
}

void stop_server(void)
{
	write_files();
	close(server_sd);
	unlink(server.sun_path);
}

int is_server(void)
{
	return server_sd != -1;
}

const char *get_server_name(void)
{
	return server.sun_path;
}

void set_server_name(const char *name)
{
	server.sun_family = AF_UNIX;
	strncpy(server.sun_path, name, sizeof(server.sun_path));
	server.sun_path[sizeof(server.sun_path)-1] = 0;
}

static void write_files(void)
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
