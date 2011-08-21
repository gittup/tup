#include "init.h"
#include "config.h"
#include "db.h"
#include "lock.h"
#include "entry.h"
#include <stdlib.h>
#include <unistd.h>

int tup_init(void)
{
	if(find_tup_dir() != 0) {
		fprintf(stderr, "No .tup directory found. Run 'tup init' at the top of your project to create the dependency filesystem.\n");
		return -1;
	}
	if(tup_lock_init() < 0) {
		return -1;
	}
	if(tup_db_open() != 0) {
		tup_lock_exit();
		return -1;
	}
	return 0;
}

void tup_cleanup(void)
{
	/* The tup_entry structures are a cache of the database, so they aren't
	 * normally freed during execution. There's also no need to go through
	 * the whole thing and clean them up manually since we can let the OS
	 * do it (we're quitting soon anyway). However, when valgrind is
	 * running it looks like there's a bunch of memory leaks, so this is
	 * done conditionally.
	 *
	 * Also close out the standard file descriptors, so valgrind doesn't
	 * complain about those as well.
	 */
	if(getenv("TUP_VALGRIND")) {
		tup_entry_clear();
		close(2);
		close(1);
		close(0);
	}
	tup_vardict_close();
	tup_db_close();
	tup_lock_exit();
	close(tup_top_fd());
}
