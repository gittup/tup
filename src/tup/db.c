#include "db.h"
#include <stdio.h>

sqlite3 *tup_db;

int tup_open_db(void)
{
	int rc;

	rc = sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != 0) {
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
	}
	return rc;
}

int tup_create_db(void)
{
	int rc;

	rc = sqlite3_open(TUP_DB_FILE, &tup_db);
	if(rc == 0) {
		printf(".tup repository initialized.\n");
	} else {
		fprintf(stderr, "Unable to create database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	return rc;
}

int tup_db_exec(char **errmsg, const char *sql, ...)
{
	int len;
	static char buf[8192];
	va_list ap;
	int rc;

	va_start(ap, sql);
	len = vsnprintf(buf, sizeof(buf), sql, ap);
	va_end(ap);
	if(len >= (signed)sizeof(buf)) {
		fprintf(stderr, "Error: SQL too big for the buffer.\n");
		return -1;
	}
	rc = sqlite3_exec(tup_db, buf, NULL, NULL, errmsg);
	return rc;
}
