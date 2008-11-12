#include "db.h"
#include <stdio.h>
#include <sqlite3.h>

struct db_node {
	tupid_t tupid;
	int type;
	int flags;
};

static sqlite3 *tup_db = NULL;
static int node_insert(const char *name, int type, int flags);
static int node_select(const char *name, struct db_node *dbn);
static int set_node_flags(tupid_t tupid, int flags);

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

tupid_t tup_db_create_node(const char *name, int type, int flags)
{
	struct db_node dbn = {-1, 0, 0};

	if(node_select(name, &dbn) < 0) {
		return -1;
	}

	if(dbn.tupid != -1) {
		if(dbn.flags & TUP_FLAGS_DELETE) {
			dbn.flags &= ~TUP_FLAGS_DELETE;
			if(set_node_flags(dbn.tupid, dbn.flags) < 0)
				return -1;
		}
		return dbn.tupid;
	}

	if(node_insert(name, type, flags) < 0)
		return -1;
	return sqlite3_last_insert_rowid(tup_db);
}

tupid_t tup_db_select_node(const char *name)
{
	struct db_node dbn = {-1, 0, 0};

	if(node_select(name, &dbn) < 0) {
		return -1;
	}

	return dbn.tupid;
}

int tup_db_set_node_flags(const char *name, int flags)
{
	struct db_node dbn = {-1, 0, 0};

	if(node_select(name, &dbn) < 0)
		return -1;
	if(dbn.tupid == -1)
		return -1;

	if(set_node_flags(dbn.tupid, flags) < 0)
		return -1;
	return 0;
}

static int node_insert(const char *name, int type, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert into node(name, type, flags) values(?, ?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 3, flags) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int node_select(const char *name, struct db_node *dbn)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, type, flags from node where name=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = 0;
	dbn->tupid = sqlite3_column_int64(stmt, 0);
	dbn->type = sqlite3_column_int64(stmt, 1);
	dbn->flags = sqlite3_column_int(stmt, 2);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int set_node_flags(tupid_t tupid, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, flags) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}
