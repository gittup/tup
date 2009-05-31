#include "memdb.h"
#include "db_util.h"
#include "array_size.h"
#include <stdio.h>

/* TODO: This should probably just be replaced by a normal balanced binary
 * tree. I tried to use tsearch/tfind/etc, but that failed miserably.
 */

int memdb_init(struct memdb *m)
{
	int x;
	int rc;
	const char *sql[] = {
		"create table node_map (id integer primary key not null, ptr integer not null)",
	};

	rc = sqlite3_open(":memory:", &m->db);
	if(rc != 0) {
		fprintf(stderr, "Unable to create in-memory database: %s\n",
			sqlite3_errmsg(m->db));
		return -1;
	}
	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(m->db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}
	for(x=0; x<ARRAY_SIZE(m->stmt); x++) {
		m->stmt[x] = NULL;
	}
	return 0;
}

int memdb_close(struct memdb *m)
{
	return db_close(m->db, m->stmt, ARRAY_SIZE(m->stmt));
}

int memdb_add(struct memdb *m, tupid_t id, void *n)
{
	int rc;
	sqlite3_stmt **stmt = &m->stmt[MEMDB_ADD];
	static char s[] = "insert into node_map(id, ptr) values(?, ?)";

	if(!*stmt) {
		if(sqlite3_prepare_v2(m->db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(m->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, id) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, (unsigned long)n) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	return 0;
}

int memdb_remove(struct memdb *m, tupid_t id)
{
	int rc;
	sqlite3_stmt **stmt = &m->stmt[MEMDB_REMOVE];
	static char s[] = "delete from node_map where id=?";

	if(!*stmt) {
		if(sqlite3_prepare_v2(m->db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(m->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, id) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	return 0;
}

int memdb_find(struct memdb *m, tupid_t id, void *p)
{
	int dbrc;
	sqlite3_stmt **stmt = &m->stmt[MEMDB_FIND];
	static char s[] = "select ptr from node_map where id=?";
	unsigned long res;
	int rc = -1;

	if(!*stmt) {
		if(sqlite3_prepare_v2(m->db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(m->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, id) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		*(void**)p = NULL;
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(m->db));
		goto out_reset;
	}

	res = sqlite3_column_int64(*stmt, 0);
	*(void**)p = (void*)res;
	rc = 0;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(m->db));
		return -1;
	}

	return rc;
}
