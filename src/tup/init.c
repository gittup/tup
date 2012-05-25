/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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
#include <stdlib.h>
#include <unistd.h>

int tup_init(void)
{
	if(find_tup_dir() != 0) {
		fprintf(stderr, "No .tup directory found. Run 'tup init' at the top of your project to create the dependency filesystem.\n");
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
	if(tup_option_init() < 0) {
		goto out_unlock;
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
