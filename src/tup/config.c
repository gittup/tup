/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _ATFILE_SOURCE
#include "config.h"
#include "compat.h"
#include "colors.h"
#include "db_types.h"
#include "progress.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static char tup_wd[PATH_MAX];
static int tup_wd_offset;
static int tup_top_len;
static int tup_sub_len;
static tupid_t tup_sub_dir_dt = -1;
static int top_fd = -1;
static int vardict_fd = -1;

int find_tup_dir(void)
{
	struct stat st;

	if(getcwd(tup_wd, sizeof(tup_wd)) == NULL) {
		perror("getcwd");
		fprintf(stderr, "tup error: Unable to get the current directory during tup initialization.\n");
		exit(1);
	}

	tup_top_len = strlen(tup_wd);
	tup_sub_len = 0;
	for(;;) {
		if(stat(".tup", &st) == 0 && S_ISDIR(st.st_mode)) {
			tup_wd_offset = tup_top_len;
			while(is_path_sep(&tup_wd[tup_wd_offset])) {
				tup_wd_offset++;
				tup_sub_len--;
			}
			tup_wd[tup_top_len] = 0;
			break;
		}
		if(chdir("..") < 0) {
			perror("chdir");
			exit(1);
		}
		while(tup_top_len > 0) {
			tup_top_len--;
			tup_sub_len++;
			if(is_path_sep(&tup_wd[tup_top_len])) {
				break;
			}
		}
		if(!tup_top_len) {
			return -1;
		}
	}
	return 0;
}

int open_tup_top(void)
{
	top_fd = open(".", O_RDONLY);
	if(top_fd < 0) {
		perror(".");
		fprintf(stderr, "tup error: Unable to open the tup root directory.\n");
		return -1;
	}
	return 0;
}

tupid_t get_sub_dir_dt(void)
{
	if(tup_sub_dir_dt < 0) {
		tup_sub_dir_dt = find_dir_tupid(get_sub_dir());
		if(tup_sub_dir_dt < 0) {
			fprintf(stderr, "Error: Unable to find tupid for working directory: '%s'\n", get_sub_dir());
		}
	}
	return tup_sub_dir_dt;
}

const char *get_tup_top(void)
{
	return tup_wd;
}

int get_tup_top_len(void)
{
	return tup_top_len;
}

const char *get_sub_dir(void)
{
	if(tup_wd[tup_wd_offset])
		return tup_wd + tup_wd_offset;
	return ".";
}

int get_sub_dir_len(void)
{
	return tup_sub_len;
}

int tup_top_fd(void)
{
	return top_fd;
}

int tup_vardict_fd(void)
{
	return vardict_fd;
}

int tup_vardict_open(void)
{
	vardict_fd = openat(tup_top_fd(), TUP_VARDICT_FILE, O_RDONLY);
	if(vardict_fd < 0) {
		/* Create vardict if it doesn't exist, since I forgot to add
		 * that to the database update part whenever I added this file.
		 * Not sure if this is the best approach, but it at least
		 * prevents a useless error message from coming up.
		 */
		if(errno == ENOENT) {
			vardict_fd = openat(tup_top_fd(), TUP_VARDICT_FILE, O_CREAT|O_RDONLY, 0666);
			if(vardict_fd < 0) {
				perror(TUP_VARDICT_FILE);
				return -1;
			}
		} else {
			perror(TUP_VARDICT_FILE);
			return -1;
		}
	}
	return 0;
}

void tup_vardict_close(void)
{
	if(vardict_fd != -1) {
		if(close(vardict_fd) < 0) {
			perror("close2(vardict_fd)");
		}
	}
	vardict_fd = -1;
}

/* Notes:
 * iserr=0 means stdout, for sub-processes that succeed
 * iserr=1 means stderr, for run-scripts
 * iserr=2 means stderr, for tup errors (eg: missing deps)
 * iserr=3 means stderr, for sub-processes that fail
 */
int display_output(int fd, int iserr, const char *name, int display_name)
{
	if(fd != -1) {
		char buf[1024];
		int rc;
		int displayed = 0;
		FILE *out = stdout;

		if(iserr)
			out = stderr;

		while(1) {
			rc = read(fd, buf, sizeof(buf));
			if(rc < 0) {
				perror("read");
				return -1;
			}
			if(rc == 0)
				break;
			if(!displayed) {
				displayed = 1;
				clear_active(out);
				if(iserr == 2) {
					/* For tup errors (eg: missing deps) */
					fprintf(stderr, " *** tup errors ***\n");
				}
				if(iserr == 1) {
					/* This is for run-scripts */
					if(display_name) {
						color_set(stderr);
						fprintf(stderr, " *** tup: stderr from command '%s%s%s%s' ***\n", color_type(TUP_NODE_CMD), color_append_normal(), name, color_end());
					}
				}
			}
			fprintf(out, "%.*s", rc, buf);
		}
	}
	return 0;
}
