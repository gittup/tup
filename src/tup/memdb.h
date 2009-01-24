#ifndef tup_memdb_h
#define tup_memdb_h

#include "tupid.h"
#include <sqlite3.h>

struct memdb {
	sqlite3 *db;
};

int memdb_init(struct memdb *m);
int memdb_add(struct memdb *m, tupid_t id, void *n);
int memdb_remove(struct memdb *m, tupid_t id);
int memdb_find(const struct memdb *m, tupid_t id, void *p);

#endif
