/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

#include "init.h"
#include "config.h"
#include "db.h"
#include "lock.h"
#include "entry.h"
#include "server.h"
#include "option.h"
#include "colors.h"
#include "privs.h"
#include "variant.h"
#include "version.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef _WIN32
#define mkdir(a,b) mkdir(a)
#endif

int tup_init(void)
{
	if(find_tup_dir() != 0) {
		fprintf(stderr, "tup %s usage: tup [args]\n", tup_version);
		fprintf(stderr, "For information on Tupfiles and other commands, see the tup(1) man page.\n");
		fprintf(stderr, "No .tup directory found. Either create a Tupfile.ini file at the top of your project, or manually run 'tup init' there.\n");
		return -1;
	}
	if(tup_entry_init() < 0) {
		return -1;
	}
	if(server_pre_init() < 0) {
		return -1;
	}
	if(tup_drop_privs() < 0) {
		goto out_err;
	}
	if(open_tup_top() < 0) {
		goto out_err;
	}
	if(tup_lock_init() < 0) {
		goto out_err;
	}
	color_init();
	if(tup_db_open() != 0) {
		goto out_unlock;
	}
	return 0;

out_unlock:
	tup_lock_exit();
out_err:
	server_post_exit();
	return -1;
}

int tup_cleanup(void)
{
	tup_db_close();
	tup_option_exit();
	tup_lock_exit();
	if(close(tup_top_fd()) < 0)
		perror("close(tup_top_fd())");
	if(server_post_exit() < 0)
		return -1;
	return 0;
}

void tup_valgrind_cleanup(void)
{
	/* The tup_entry structures are a cache of the database, so they aren't
	 * normally freed during execution. There's also no need to go through
	 * the whole thing and clean them up manually since we can let the OS
	 * do it (we're quitting soon anyway). However, when valgrind is
	 * running it looks like there's a bunch of memory leaks, so this is
	 * done conditionally.
	 */
	if(getenv("TUP_VALGRIND")) {
		tup_entry_clear();
		variants_free();

		/* Also close out the standard file descriptors, so valgrind
		 * doesn't complain about those as well. The outputs need to be
		 * flushed, otherwise 'tup options | grep foo' will not see the
		 * output from tup. This is done here rather than tup_cleanup()
		 * because other parts of tup (such as flush()) will call
		 * cleanup and then init again. We only want to close the
		 * standard descriptors once though, so we don't impact other
		 * file descriptors that may have been opened.
		 */
		fflush(stdout);
		fflush(stderr);
		close(STDERR_FILENO);
		close(STDOUT_FILENO);
		close(STDIN_FILENO);
	}
}

static int mkdirtree(const char *dirname)
{
	char *dirpart = strdup(dirname);
	char *p;

	if(!dirpart) {
		perror("strdup");
		return -1;
	}

	p = dirpart;
	while(1) {
		char *slash = p;
		char slash_found = 0;

		while(*slash && !is_path_sep(slash)) {
			slash++;
		}
		if(*slash) {
			slash_found = *slash;
			*slash = 0;
		}
		if(mkdir(dirpart, 0777) < 0) {
			if(errno != EEXIST) {
				perror(dirpart);
				fprintf(stderr, "tup error: Unable to create directory '%s' for a tup repository.\n", dirname);
				return -1;
			}
		}
		if(slash_found) {
			*slash = slash_found;
			p = slash + 1;
		} else {
			break;
		}
	}
	free(dirpart);
	return 0;
}

int init_command(int argc, char **argv)
{
	int x;
	int db_sync = 1;
	int force_init = 0;
	int fd;
	const char *dirname = NULL;

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "--no-sync") == 0) {
			db_sync = 0;
		} else if(strcmp(argv[x], "--force") == 0) {
			/* force should only be used for tup/test */
			force_init = 1;
		} else {
			if(dirname) {
				fprintf(stderr, "tup error: Expected only one directory name for 'tup init', but got '%s' and '%s'\n", dirname, argv[x]);
				return -1;
			}
			dirname = argv[x];
		}
	}

	if(dirname) {
		if(mkdirtree(dirname) < 0)
			return -1;
	} else {
		dirname = ".";
	}

	fd = open(dirname, O_RDONLY);
	if(fd < 0) {
		perror(dirname);
		return -1;
	}

	if(!force_init && find_tup_dir() == 0) {
		char wd[PATH_MAX];
		if(getcwd(wd, sizeof(wd)) == NULL) {
			perror("getcwd");
			fprintf(stderr, "tup warning: database already exists somewhere up the tree.\n");
		} else {
			fprintf(stderr, "tup warning: database already exists in directory: %s\n", wd);
		}
		close(fd);
		return 0;
	}

	if(fchdir(fd) < 0) {
		perror("fchdir");
		close(fd);
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}

	if(mkdir(TUP_DIR, 0777) != 0) {
		perror(TUP_DIR);
		return -1;
	}

	if(tup_db_create(db_sync, 0) != 0) {
		return -1;
	}

	if(!db_sync) {
		FILE *f = fopen(TUP_OPTIONS_FILE, "w");
		if(!f) {
			perror(TUP_OPTIONS_FILE);
			return -1;
		}
		fprintf(f, "[db]\n");
		fprintf(f, "\tsync = false\n");
		fclose(f);
	}
	return 0;
}
