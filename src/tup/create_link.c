#include "fileio.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int link_cb(void *id, int argc, char **argv, char **col);

int create_link(const new_tupid_t a, const new_tupid_t b)
{
	new_tupid_t id = -1;
	int rc;

	rc = tup_db_select(link_cb, &id,
			   "select from_id from link where from_id=%lli and to_id=%lli", a, b);
	if(rc == 0 && id != -1)
		return 0;

	rc = tup_db_exec("insert into link(from_id, to_id) values(%lli, %lli)",
			 a, b);
	if(rc == 0)
		return 0;
	return -1;
}

int find_link(const char *from, const char *to)
{
	new_tupid_t id = -1;
	int rc;

	rc = tup_db_select(link_cb, &id,
			   "select from_id from link where from_id in (select id from node where name='%q') and to_id in (select id from node where name='%q')",
			   from, to);
	if(rc == 0 && id != -1)
		return 0;

	return -1;
}

static int link_cb(void *id, int argc, char **argv, char **col)
{
	int x;
	new_tupid_t *iptr = id;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "from_id") == 0) {
			*iptr = atoll(argv[x]);
			return 0;
		}
	}
	return -1;
}
