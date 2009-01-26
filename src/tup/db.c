#define _ATFILE_SOURCE
#include "db.h"
#include "array_size.h"
#include "list.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sqlite3.h>

#define DB_VERSION 1

static sqlite3 *tup_db = NULL;
static int node_insert(tupid_t dt, const char *name, int len,
		       int type, int flags);
static int node_select(tupid_t dt, const char *name, int len,
		       struct db_node *dbn);

static int link_insert(tupid_t a, tupid_t b);
static int no_sync(void);
static int delete_dir(tupid_t dt);
static int get_recurse_dirs(tupid_t dt, struct list_head *list);

struct id_entry {
	struct list_head list;
	tupid_t id;
};

int tup_db_open(void)
{
	int rc;
	int version;

	rc = sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != 0) {
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	if(tup_db_config_get_int("db_sync") == 0) {
		if(no_sync() < 0)
			return -1;
	}
	version = tup_db_config_get_int("db_version");
	if(version < 0) {
		fprintf(stderr, "Error getting .tup/db version.\n");
		return -1;
	}
	if(version != DB_VERSION) {
		fprintf(stderr, "Error: Database version %i not compatible with %i\n", version, DB_VERSION);
		return -1;
	}

	/* TODO: better to figure out concurrency access issues? Maybe a full
	 * on flock on the db would work?
	 */
	sqlite3_busy_timeout(tup_db, 500);
	return rc;
}

int tup_db_close(void)
{
	sqlite3_stmt *stmt;

	while((stmt = sqlite3_next_stmt(tup_db, 0)) !=0) {
		sqlite3_finalize(stmt);
	}

	if(sqlite3_close(tup_db) != 0) {
		fprintf(stderr, "Unable to close database: %s\n",
			sqlite3_errmsg(tup_db));
		return -1;
	}
	return 0;
}

int tup_db_create(int db_sync)
{
	int rc;
	int x;
	const char *sql[] = {
		"create table node (id integer primary key not null, dir integer not null, type integer not null, flags integer not null, name varchar(4096))",
		"create table link (from_id integer, to_id integer)",
		"create table var (id integer primary key not null, value varchar(4096))",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		"create index node_dir_index on node(dir, name)",
		"create index node_flags_index on node(flags)",
		"create index link_index on link(from_id)",
		"create index link_index2 on link(to_id)",
		"insert into config values('show_progress', 1)",
		"insert into config values('keep_going', 0)",
		"insert into config values('db_sync', 1)",
		"insert into config values('db_version', 0)",
		"insert into node values(1, 0, 2, 0, '.')",
		"insert into node values(2, 1, 2, 0, '@')",
	};

	rc = sqlite3_open(TUP_DB_FILE, &tup_db);
	if(rc == 0) {
		printf(".tup repository initialized.\n");
	} else {
		fprintf(stderr, "Unable to create database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	if(db_sync == 0) {
		if(no_sync() < 0)
			return -1;
	}

	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(tup_db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}

	if(db_sync == 0) {
		if(tup_db_config_set_int("db_sync", 0) < 0)
			return -1;
	}
	if(tup_db_config_set_int("db_version", DB_VERSION) < 0)
		return -1;

	return 0;
}

int tup_db_begin(void)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "begin";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

int tup_db_rollback(void)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "rollback";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

tupid_t tup_db_create_node(tupid_t dt, const char *name, int type, int flags)
{
	return tup_db_create_node_part(dt, name, -1, type, flags, NULL);
}

tupid_t tup_db_create_node_part(tupid_t dt, const char *name, int len, int type,
				int flags, int *node_created)
{
	struct db_node dbn;

	if(node_created)
		*node_created = 0;

	if(node_select(dt, name, len, &dbn) < 0) {
		return -1;
	}

	if(dbn.tupid != -1) {
		if(dbn.type != type) {
			fprintf(stderr, "Error: Attempt to insert node '%s' with type %i, which already exists as type %i\n", name, type, dbn.type);
			return -1;
		}
		if(dbn.flags & TUP_FLAGS_DELETE) {
			dbn.flags &= ~TUP_FLAGS_DELETE;
			if(tup_db_set_flags_by_id(dbn.tupid, dbn.flags) < 0)
				return -1;
		}
		return dbn.tupid;
	}

	if(node_insert(dt, name, len, type, flags) < 0)
		return -1;
	if(node_created)
		*node_created = 1;
	return sqlite3_last_insert_rowid(tup_db);
}

tupid_t tup_db_create_dup_node(tupid_t dt, const char *name, int type, int flags)
{
	if(node_insert(dt, name, -1, type, flags) < 0)
		return -1;
	return sqlite3_last_insert_rowid(tup_db);
}

tupid_t tup_db_select_node(tupid_t dt, const char *name)
{
	struct db_node dbn;

	if(node_select(dt, name, -1, &dbn) < 0) {
		return -1;
	}

	return dbn.tupid;
}

tupid_t tup_db_select_dbn(tupid_t dt, const char *name, struct db_node *dbn)
{
	if(node_select(dt, name, -1, dbn) < 0)
		return -1;

	dbn->dt = dt;
	dbn->name = name;

	return dbn->tupid;
}

int tup_db_select_flags(tupid_t tupid)
{
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select flags from node where id=?";
	int flags = -1;

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr, "Node not found: %lli\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	flags = sqlite3_column_int(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return flags;
}

tupid_t tup_db_select_node_part(tupid_t dt, const char *name, int len)
{
	struct db_node dbn;

	if(node_select(dt, name, len, &dbn) < 0) {
		return -1;
	}

	return dbn.tupid;
}

int tup_db_select_node_by_flags(int (*callback)(void *, struct db_node *),
				void *arg, int flags)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, dir, name, type from node where flags=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, flags) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

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

		dbn.tupid = sqlite3_column_int64(stmt, 0);
		dbn.dt = sqlite3_column_int64(stmt, 1);
		dbn.name = (const char *)sqlite3_column_text(stmt, 2);
		dbn.type = sqlite3_column_int(stmt, 3);
		dbn.flags = flags;

		if((rc = callback(arg, &dbn)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir(int (*callback)(void *, struct db_node *), void *arg,
			   tupid_t dt)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, name, type, flags from node where dir=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

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

		dbn.tupid = sqlite3_column_int64(stmt, 0);
		dbn.dt = dt;
		dbn.name = (const char *)sqlite3_column_text(stmt, 1);
		dbn.type = sqlite3_column_int(stmt, 2);
		dbn.flags = sqlite3_column_int(stmt, 3);

		if(callback(arg, &dbn) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir_glob(int (*callback)(void *, struct db_node *),
				void *arg, tupid_t dt, const char *glob)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, name, type, flags from node where dir=? and type=? and flags!=? and name glob ?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, TUP_NODE_FILE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 3, TUP_FLAGS_DELETE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 4, glob, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

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

		dbn.tupid = sqlite3_column_int64(stmt, 0);
		dbn.dt = dt;
		dbn.name = (const char *)sqlite3_column_text(stmt, 1);
		dbn.type = sqlite3_column_int(stmt, 2);
		dbn.flags = sqlite3_column_int(stmt, 3);

		if(callback(arg, &dbn) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_set_flags_by_name(tupid_t dt, const char *name, int flags)
{
	struct db_node dbn;

	if(node_select(dt, name, -1, &dbn) < 0)
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

int tup_db_delete_dir(tupid_t dt)
{
	printf("[35m Delete dir: %lli[0m\n", dt);
	if(tup_db_set_flags_by_id(dt, TUP_FLAGS_DELETE) < 0)
		return -1;
	return delete_dir(dt);
}

int tup_db_opendir(tupid_t dt)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select dir, name from node where id=?";
	tupid_t parent;
	char *path;
	int fd;

	if(dt == 0) {
		fprintf(stderr, "Error: Trying to tup_db_opendir(0)\n");
		return -1;
	}
	if(dt == 1) {
		return open(".", O_RDONLY);
	}
	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -ENOENT;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	parent = sqlite3_column_int64(stmt, 0);
	path = strdup((const char *)sqlite3_column_text(stmt, 1));
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	fd = tup_db_opendir(parent);
	if(fd < 0)
		return -1;

	rc = openat(fd, path, O_RDONLY);
	if(rc < 0) {
		if(errno == ENOENT)
			rc = -ENOENT;
		else
			perror(path);
	}
	close(fd);
	free(path);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

tupid_t tup_db_parent(tupid_t tupid)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select dir from node where id=?";
	tupid_t parent;

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_DONE) {
		parent = -1;
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		parent = -1;
		goto out_reset;
	}

	parent = sqlite3_column_int64(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return parent;
}

static int delete_dir(tupid_t dt)
{
	LIST_HEAD(subdir_list);
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where dir=?";

	printf("[35m delete dir: %lli[0m\n", dt);
	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, TUP_FLAGS_DELETE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, dt) != 0) {
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

	if(get_recurse_dirs(dt, &subdir_list) < 0)
		return -1;
	while(!list_empty(&subdir_list)) {
		struct id_entry *ide = list_entry(subdir_list.next,
						  struct id_entry, list);
		delete_dir(ide->id);
		list_del(&ide->list);
		free(ide);
	}

	return 0;
}

static int get_recurse_dirs(tupid_t dt, struct list_head *list)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id from node where dir=? and type=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct id_entry *ide;

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

		ide = malloc(sizeof *ide);
		if(ide == NULL) {
			perror("malloc");
			return -1;
		}
		ide->id = sqlite3_column_int64(stmt, 0);
		list_add(&ide->list, list);
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

int tup_db_or_dircmd_flags(tupid_t parent, int flags, int type)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[]="update node set flags=flags|? where dir=? and type=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
	if(sqlite3_bind_int(stmt, 3, type) != 0) {
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

int tup_db_set_cmd_output_flags(tupid_t parent, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where id in (select to_id from link where from_id in (select id from node where dir=? and type=?))";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
	if(sqlite3_bind_int(stmt, 3, TUP_NODE_CMD) != 0) {
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

int tup_db_set_cmd_flags_by_output(tupid_t output, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where id in (select from_id from link where to_id=?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, flags) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, output) != 0) {
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

int tup_db_select_node_by_link(int (*callback)(void *, struct db_node *),
			       void *arg, tupid_t tupid)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, dir, name, type, flags from node where id in (select to_id from link where from_id=?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

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

		dbn.tupid = sqlite3_column_int64(stmt, 0);
		dbn.dt = sqlite3_column_int64(stmt, 1);
		dbn.name = (const char *)sqlite3_column_text(stmt, 2);
		dbn.type = sqlite3_column_int(stmt, 3);
		dbn.flags = sqlite3_column_int(stmt, 4);

		if(callback(arg, &dbn) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_set_dependent_dir_flags(tupid_t dt, int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "update node set flags=? where id in (select to_id from link where from_id=?) and type=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, flags) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(stmt, 2, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 3, TUP_NODE_DIR) != 0) {
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

int tup_db_config_set_string(const char *lval, const char *rval)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert or replace into config values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, rval, -1, SQLITE_STATIC) != 0) {
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

int tup_db_config_get_string(char **res, const char *lval, const char *def)
{
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select rval from config where lval=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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
		*res = strdup(def);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		*res = NULL;
		goto out_reset;
	}

	*res = strdup((const char *)sqlite3_column_text(stmt, 0));

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(*res)
		return 0;
	return -1;
}

int tup_db_set_var(tupid_t tupid, const char *value)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert or replace into var values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC) != 0) {
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

tupid_t tup_db_get_var(const char *var, int varlen, char **dest)
{
	int dbrc;
	int len;
	tupid_t tupid = -1;
	const char *value;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select var.id, value, length(value) from var, node where node.dir=? and node.name=? and node.id=var.id";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable '%.*s' not found in .tup/db.\n",
			varlen, var);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(stmt, 2);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(stmt, 1);
	if(!value) {
		goto out_reset;
	}
	memcpy(*dest, value, len);
	*dest += len;

	tupid = sqlite3_column_int64(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return tupid;
}

int tup_db_get_var_id(tupid_t tupid, char **dest)
{
	int rc = -1;
	int dbrc;
	int len;
	const char *value;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select value, length(value) from var where var.id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable id %lli not found in .tup/db.\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(stmt, 1);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(stmt, 0);
	if(!value) {
		goto out_reset;
	}
	*dest = malloc(len + 1);
	if(!*dest) {
		perror("malloc");
		goto out_reset;
	}
	memcpy(*dest, value, len);
	(*dest)[len] = 0;
	rc = 0;

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_get_varlen(const char *var, int varlen)
{
	int rc = -1;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select length(value) from var, node where node.dir=? and node.name=? and node.id=var.id";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable '%.*s' not found in .tup/db.\n",
			varlen, var);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
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

tupid_t tup_db_write_var(const char *var, int varlen, int fd)
{
	int dbrc;
	int len;
	const char *value;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select var.id, value, length(value) from var, node where node.dir=? and node.name=? and node.id=var.id";
	tupid_t tupid = -1;

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable '%.*s' not found in .tup/db.\n",
			varlen, var);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(stmt, 2);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(stmt, 1);
	if(!value) {
		goto out_reset;
	}

	if(write(fd, value, len) == len)
		tupid = sqlite3_column_int64(stmt, 0);

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return tupid;
}

int tup_db_var_foreach(int (*callback)(void *, const char *var, const char *value), void *arg)
{
	int rc = -1;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select name, value from var, node where node.dir=? and node.id=var.id";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		const char *var;
		const char *value;

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

		var = (const char *)sqlite3_column_text(stmt, 0);
		value = (const char *)sqlite3_column_text(stmt, 1);

		if((rc = callback(arg, var, value)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int node_insert(tupid_t dt, const char *name, int len, int type,
		       int flags)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert into node(dir, type, flags, name) values(?, ?, ?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
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
	if(sqlite3_bind_text(stmt, 4, name, len, SQLITE_STATIC) != 0) {
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

static int node_select(tupid_t dt, const char *name, int len,
		       struct db_node *dbn)
{
	int rc;
	int dbrc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select id, type, flags from node where dir=? and name=?";

	dbn->tupid = -1;
	dbn->dt = -1;
	dbn->name = NULL;
	dbn->type = 0;
	dbn->flags = 0;

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(stmt, 2, name, len, SQLITE_STATIC) != 0) {
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

	if(a == b) {
		fprintf(stderr, "Error: Attempt made to link a node to itself (%lli)\n", a);
		return -1;
	}

	if(!stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
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

static int no_sync(void)
{
	char *errmsg;
	char sql[] = "PRAGMA synchronous=OFF";

	if(sqlite3_exec(tup_db, sql, NULL, NULL, &errmsg) != 0) {
		fprintf(stderr, "SQL error: %s\nQuery was: %s",
			errmsg, sql);
		return -1;
	}
	return 0;
}
