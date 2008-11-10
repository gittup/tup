#include "db.h"
#include <stdio.h>

sqlite3 *tup_db = NULL;

int tup_open_db(void)
{
	int rc;

	rc = sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != 0) {
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	/* TODO: better to figure out concurrency access issues? Maybe a full
	 * on flock on the db would work?
	 */
	sqlite3_busy_timeout(tup_db, 500);
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

int tup_db_exec(const char *sql, ...)
{
	va_list ap;
	int rc;
	char *buf;
	char *errmsg;

	if(!tup_db) {
		fprintf(stderr, "Error: tup_db not opened.\n");
		return -1;
	}

	va_start(ap, sql);
	buf = sqlite3_vmprintf(sql, ap);
	va_end(ap);

	rc = sqlite3_exec(tup_db, buf, NULL, NULL, &errmsg);
	if(rc != 0) {
		fprintf(stderr, "SQL exec error: %s\nQuery was: %s\n",
			errmsg, buf);
		sqlite3_free(errmsg);
	}
	sqlite3_free(buf);
	return rc;
}

int tup_db_select(int (*callback)(void *, int, char **, char **),
		  void *arg, const char *sql, ...)
{
	va_list ap;
	int rc;
	char *buf;
	char *errmsg;

	if(!tup_db) {
		fprintf(stderr, "Error: tup_db not opened.\n");
		return -1;
	}

	va_start(ap, sql);
	buf = sqlite3_vmprintf(sql, ap);
	va_end(ap);

	rc = sqlite3_exec(tup_db, buf, callback, arg, &errmsg);
	if(rc != 0) {
		fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
			errmsg, buf);
		sqlite3_free(errmsg);
	}
	sqlite3_free(buf);
	return rc;
}
