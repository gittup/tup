#ifndef tup_memdb_h
#define tup_memdb_h

#include "tupid.h"
#include "sqlite3/sqlite3.h"

enum {
	MEMDB_ADD,
	MEMDB_REMOVE,
	MEMDB_FIND,
	MEMDB_NUM_STATEMENTS,
};

struct memdb {
	sqlite3 *db;
	sqlite3_stmt *stmt[MEMDB_NUM_STATEMENTS];
};

int memdb_init(struct memdb *m);
int memdb_close(struct memdb *m);
int memdb_add(struct memdb *m, tupid_t id, void *n);
int memdb_remove(struct memdb *m, tupid_t id);
int memdb_find(struct memdb *m, tupid_t id, void *p);

#endif
