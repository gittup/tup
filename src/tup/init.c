#include "init.h"
#include "config.h"
#include "db.h"
#include "lock.h"

int tup_init(void)
{
	if(find_tup_dir() != 0) {
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
	tup_db_reclaim_ghosts();
	tup_db_write_vars();
	tup_db_close();
	tup_lock_exit();
}
