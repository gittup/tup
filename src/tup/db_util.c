#include "db_util.h"
#include <stdio.h>

int db_close(sqlite3 *db, sqlite3_stmt **stmts, int num)
{
	int x;
	for(x=0; x<num; x++) {
		if(stmts[x])
			sqlite3_finalize(stmts[x]);
	}

	if(sqlite3_close(db) != 0) {
		fprintf(stderr, "Unable to close database: %s\n",
			sqlite3_errmsg(db));
		return -1;
	}
	return 0;
}
