#include "fileio.h"
#include "db.h"
#include <stdio.h>

static int nd_cb(void *arg, int argc, char **argv, char **col);

int num_dependencies(new_tupid_t tupid)
{
	int x = 0;

	if(tup_db_select(nd_cb, &x, "select from_id from link where to_id=%lli",
			 tupid) != 0)
		return -1;

	return x;
}

static int nd_cb(void *arg, int argc, char **argv, char **col)
{
	int *iptr = arg;
	if(argc) {}
	if(argv) {}
	if(col) {}

	(*iptr)++;
	return 0;
}
