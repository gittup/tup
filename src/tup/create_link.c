#include "fileio.h"
#include "debug.h"
#include "db.h"

#include <stdio.h>

int create_link(const new_tupid_t a, const new_tupid_t b)
{
	int rc;
	char *errmsg;

	rc = tup_db_exec(&errmsg,
			 "insert into link(from_id, to_id) values(%lli, %lli)",
			 a, b);
	if(rc == 0)
		return 0;

	fprintf(stderr, "SQL insert error: %s\n", errmsg);
	return -1;
}
