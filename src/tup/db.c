#include "db.h"
#include <stdio.h>
#include <sqlite3.h>

#define ARRAY_SIZE(n) ((signed)(sizeof(n) / sizeof(n[0])))

struct db_node {
	tupid_t tupid;
	int type;
	int flags;
};

static sqlite3 *tup_db = NULL;
static int node_insert(const char *name, int type, int flags);
static int node_select(const char *name, struct db_node *dbn);

static int link_insert(tupid_t a, tupid_t b);
static int cmdlink_insert(tupid_t a, tupid_t b);

int tup_db_open(void)
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

int tup_db_create(void)
{
	int rc;
	int x;
	const char *sql[] = {
		"create table node (id integer primary key not null, name varchar(4096) unique, type integer not null, flags integer not null)",
		"create table cmdlink (from_id integer, to_id integer)",
		"create table link (from_id integer, to_id integer)",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		/* TODO: Not needed because name is unique? */
		/*"create index node_index on node(name)",*/
		"create index node_flags_index on node(flags)",
		"create index link_index on link(from_id)",
		"create index link_index2 on link(to_id)",
		"create index cmdlink_index on cmdlink(from_id)",
		"create index cmdlink_index2 on cmdlink(to_id)",
	};

	rc = sqlite3_open(TUP_DB_FILE, &tup_db);
	if(rc == 0) {
		printf(".tup repository initialized.\n");
	} else {
		fprintf(stderr, "Unable to create database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(tup_db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}

	return rc;
}

int tup_db_begin(void)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "begin";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
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

int tup_db_commit(void)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "commit";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
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
			if(tup_db_set_flags_by_id(dbn.tupid, dbn.flags) < 0)
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

int tup_db_set_flags_by_name(const char *name, int flags)
{
	struct db_node dbn = {-1, 0, 0};

	if(node_select(name, &dbn) < 0)
		return -1;
	if(dbn.tupid == -1)
		return -1;

	if(tup_db_set_flags_by_id(dbn.tupid, flags) < 0)
		return -1;
	return 0;
}

int tup_db_set_flags_by_id(tupid_t tupid, int flags)
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

int tup_db_delete_node(tupid_t tupid)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "delete from node where id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
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

int tup_db_create_link(tupid_t a, tupid_t b)
{
	if(tup_db_link_exists(a, b) == 0)
		return 0;
	if(link_insert(a, b) < 0)
		return -1;
	return 0;
}

int tup_db_link_exists(tupid_t a, tupid_t b)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select to_id from link where from_id=? and to_id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc == SQLITE_DONE) {
		return -1;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_delete_links(tupid_t tupid)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "delete from link where from_id=? or to_id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
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

int tup_db_create_cmdlink(tupid_t a, tupid_t b)
{
	if(tup_db_cmdlink_exists(a, b) == 0)
		return 0;
	if(cmdlink_insert(a, b) < 0)
		return -1;
	return 0;
}

int tup_db_cmdlink_exists(tupid_t a, tupid_t b)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select to_id from cmdlink where from_id=? and to_id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc == SQLITE_DONE) {
		return -1;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_delete_cmdlinks(tupid_t tupid)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "delete from cmdlink where from_id=? or to_id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
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

int tup_db_set_cmdchild_flags(tupid_t parent, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where id in (select to_id from cmdlink where from_id=?)";

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
	if(sqlite3_bind_int64(stmt, 2, parent) != 0) {
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

int tup_db_config_set_int(const char *lval, int x)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert or replace into config values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, x) != 0) {
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

int tup_db_config_get_int(const char *lval)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select rval from config where lval=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
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

static int link_insert(tupid_t a, tupid_t b)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert into link(from_id, to_id) values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, b) != 0) {
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

static int cmdlink_insert(tupid_t a, tupid_t b)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert into cmdlink(from_id, to_id) values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, b) != 0) {
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
