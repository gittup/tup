#include "lock.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/file.h>

static int obj_lock;

int tup_lock_init(void)
{
	obj_lock = open(TUP_OBJECT_LOCK, O_RDONLY);
	if(obj_lock < 0) {
		perror(TUP_OBJECT_LOCK);
		return -1;
	}

	if(flock(obj_lock, LOCK_SH) < 0) {
		perror("flock");
		return -1;
	}
	return 0;
}

void tup_lock_exit(void)
{
	flock(obj_lock, LOCK_UN);
	close(obj_lock);
}

int tup_obj_lock(void)
{
	return obj_lock;
}
