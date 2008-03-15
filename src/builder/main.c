#define _GNU_SOURCE /* TODO: asprintf */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/flist.h"

int update(const tupid_t tupid, char type);
static const char *file_ext(const char *filename, int size);
static const char *find_last(const char *string, int size, char c);

int update(const tupid_t tupid, char type)
{
	int fd;
	struct stat buf;
	char *name;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(tupfilename + 12, tupid);
	if(stat(tupfilename, &buf) != 0) {
		perror(tupfilename);
		return -1;
	}

	name = malloc(buf.st_size);
	if(!name) {
		perror("malloc");
		return -1;
	}

	fd = open(tupfilename, O_RDONLY);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	read(fd, name, buf.st_size-1);
	name[buf.st_size-1] = 0;
	close(fd);

	if(type & TUP_CREATE) {
		const char *ext;
		ext = file_ext(name, buf.st_size);
		if(strcmp(ext, "c") == 0) {
			tupid_t obj_tupid;

			name[buf.st_size - 2] = 'o';
			if(create_name_file(name) < 0)
				return -1;
			tupid_from_filename(obj_tupid, name);
			if(create_primary_link(tupid, obj_tupid) < 0)
				return -1;
		} else if(strcmp(ext, "o") == 0) {
			const char *slash;
			char *prog_name;
			char default_prog_name[] = "prog_";
			tupid_t prog_tupid;

			slash = find_last(name, buf.st_size, '/');
			if(slash) {
				int len;
				char *p;

				/* +6 is from "prog_" and '\0' */
				len = slash - name + 6;
				prog_name = malloc(len);
				if(!prog_name) {
					perror("malloc");
					return -1;
				}
				strcpy(prog_name, "prog_");
				memcpy(prog_name + 5, name, slash - name);
				for(p=prog_name; *p; p++) {
					if(*p == '/')
						*p = '_';
				}
			} else {
				prog_name = default_prog_name;
			}

			if(create_name_file(prog_name) < 0)
				return -1;
			tupid_from_filename(prog_tupid, prog_name);
			if(create_primary_link(tupid, prog_tupid) < 0)
				return -1;
		} else {
			printf("Ignore create: '%s'\n", name);
		}
	}
	if(type & TUP_MODIFY || type & TUP_DELETE) {
		const char *ext;
		ext = file_ext(name, buf.st_size);
		if(strcmp(ext, "o") == 0) {
			int pid;
			int status;
			char *cfile;
			struct stat st;
			cfile = strdup(name);
			if(!cfile) {
				perror("strdup");
				return -1;
			}
			cfile[buf.st_size - 2] = 'c';
			if(stat(cfile, &st) < 0 || !S_ISREG(st.st_mode)) {
				if(!(type & TUP_DELETE)) {
					fprintf(stderr, "Warning: C file '%s' is missing and no delete flag is set!\n", cfile);
				}
				unlink(name);
				return -77;
			}
			printf("  CC      %s\n", cfile);
			pid = fork();
			if(pid < 0) {
				perror("fork");
				return -1;
			}
			if(pid == 0) {
				execlp("wrapper", "wrapper", "gcc", "-I.", "-c", cfile, "-o", name, NULL);
				perror("execlp");
				exit(1);
			}
			wait(&status);
			if(WIFEXITED(status)) {
				if(WEXITSTATUS(status) == 0)
					return 0;
				fprintf(stderr, "Error: gcc failed with %i\n",
					WEXITSTATUS(status));
				return -WEXITSTATUS(status);
			}
			fprintf(stderr, "Error: gcc didn't return.\n");
			return -1;
		} else if(strncmp(name, "prog_", 5) == 0) {
			struct flist f;
			char *path;
			char **objects;
			int count = 0;
			int x;
			int pid;
			int status;
			char *p;

			if(name[5]) {
				path = strdup(name + 5);
				if(!path) {
					perror("strdup");
					return -1;
				}
				for(p=path; *p != 0; p++) {
					if(*p == '_')
						*p = '/';
				}
			} else {
				path = strdup(".");
				if(!path) {
					perror("strdup");
					return -1;
				}
			}

			printf("Path: '%s'\n", path);
			flist_foreach(&f, path) {
				int len = strlen(f.filename);
				if(len > 2 &&
				   f.filename[len-2] == '.' &&
				   f.filename[len-1] == 'o') {
					count++;
				}
			}
			if(!count) {
				if(!(type & TUP_DELETE)) {
					fprintf(stderr, "Warning: No objects are present and delete flag is not set!\n");
				}
				unlink(name);
				return -77;
			}
			objects = malloc(sizeof(*objects) * (count + 6));
			if(!objects) {
				perror("malloc");
				return -1;
			}

			objects[0] = strdup("wrapper");
			objects[1] = strdup("ld");
			objects[2] = strdup("-r");
			objects[3] = strdup("-o");
			if(!objects[0] || !objects[1] || !objects[2] || !objects[3]) {
				perror("strdup");
				return -1;
			}
			objects[4] = name;
			objects[count + 5] = NULL;

			x = 5;
			flist_foreach(&f, path) {
				int len = strlen(f.filename);
				if(len > 2 &&
				   f.filename[len-2] == '.' &&
				   f.filename[len-1] == 'o') {
					if(asprintf(&objects[x], "%s/%s", path, f.filename) < 0) {
						perror("asprintf");
						return -1;
					}
					x++;
				}
			}
			printf("  LD-r    %s [%s]\n", name, path);
			pid = fork();
			if(pid < 0) {
				perror("fork");
				return -1;
			}
			if(pid == 0) {
				execvp("wrapper", objects);
				perror("execvp");
				exit(1);
			}
			wait(&status);
			if(WIFEXITED(status)) {
				if(WEXITSTATUS(status) == 0)
					return 0;
				fprintf(stderr, "Error: ld failed with %i\n",
					WEXITSTATUS(status));
				return -WEXITSTATUS(status);
			}
			fprintf(stderr, "Error: ld didn't return.\n");
			return -1;
		} else {
			printf("Ignore modify: '%s'\n", name);
			if(type & TUP_DELETE)
				return -77;
		}
	}

	return 0;
}

static const char *file_ext(const char *filename, int size)
{
	const char *p;

	for(p = filename + size - 1; p >= filename; p--) {
		if(*p == '.')
			return p+1;
	}
	return filename;
}

static const char *find_last(const char *string, int size, char c)
{
	const char *p;
	for(p = string + size - 1; p >= string; p--) {
		if(*p == c)
			return p;
	}
	return NULL;
}
