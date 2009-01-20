#include "vardb.h"
#include "db.h"
#include "array_size.h"
#include <stdio.h>
#include <string.h>

static int append(struct vardb *v, const char *var, const char *value);
static int var_exists(struct vardb *v, const char *var);

int vardb_init(struct vardb *v)
{
	int x;
	int rc;
	const char *sql[] = {
		"create table vars (var varchar(256) primary key not null, value varchar(4096))",
	};

	rc = sqlite3_open(":memory:", &v->db);
	if(rc != 0) {
		fprintf(stderr, "Unable to create in-memory database: %s\n",
			sqlite3_errmsg(v->db));
		return -1;
	}
	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(v->db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}

	return 0;
}

int vardb_set(struct vardb *v, const char *var, const char *value)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert or replace into vars(var, value) values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, var, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return 0;
}

int vardb_append(struct vardb *v, const char *var, const char *value)
{
	int rc;

	if(var_exists(v, var) == 0)
		rc = append(v, var, value);
	else
		rc = vardb_set(v, var, value);
	return rc;
}

int vardb_len(struct vardb *v, const char *var, int varlen)
{
	int rc = 0;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select length(value) from vars where var=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr, "Error: Variable '%.*s' not found.\n",
			varlen, var);
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return rc;
}

int vardb_get(struct vardb *v, const char *var, int varlen, char **dest)
{
	int rc = 0;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select value, length(value) from vars where var=?";
	int valen;
	const char *value;

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr, "Error: Variable '%.*s' not found.\n",
			varlen, var);
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
		rc = -1;
		goto out_reset;
	}

	valen = sqlite3_column_int(stmt, 1);
	if(valen < 0) {
		rc = -1;
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(stmt, 0);
	if(!value) {
		rc = -1;
		goto out_reset;
	}
	memcpy(*dest, value, valen);
	*dest += valen;

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return rc;
}

int vardb_dump(struct vardb *v)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select * from vars";

	printf("Variables:\n");

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	while(1) {
		rc = sqlite3_step(stmt);
		if(rc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(rc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
			rc = -1;
			goto out_reset;
		}

		printf(" - '%s' = '%s'\n", sqlite3_column_text(stmt, 0),
		       sqlite3_column_text(stmt, 1));
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return 0;
}

int append(struct vardb *v, const char *var, const char *value)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update vars set value=value||' '||? where var=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, var, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return 0;
}

static int var_exists(struct vardb *v, const char *var)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select var from vars where var=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(v->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(v->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, var, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}
	if(rc == SQLITE_DONE) {
		return -1;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(v->db));
		return -1;
	}

	return 0;
}
