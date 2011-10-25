#include "init.h"
#include "config.h"
#include "db.h"
#include "lock.h"
#include "entry.h"
#include "server.h"
#include "option.h"
#include "colors.h"
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
	/* The tup_entry structures are a cache of the database, so they aren't
	 * normally freed during execution. There's also no need to go through
	 * the whole thing and clean them up manually since we can let the OS
	 * do it (we're quitting soon anyway). However, when valgrind is
	 * running it looks like there's a bunch of memory leaks, so this is
	 * done conditionally.
	 */
	if(getenv("TUP_VALGRIND")) {
		tup_entry_clear();
	}
	tup_vardict_close();
	tup_db_close();
	tup_option_exit();
	tup_lock_exit();
	close(tup_top_fd());
	if(server_post_exit() < 0)
		return -1;
	return 0;
}
