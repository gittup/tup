#ifndef tup_vardb_h
#define tup_vardb_h

#include <sqlite3.h>

enum {
	VARDB_SET,
	VARDB_LEN,
	VARDB_GET,
	VARDB_DUMP,
	_VARDB_APPEND,
	_VARDB_EXISTS,
	VARDB_NUM_STATEMENTS
};

struct vardb {
	sqlite3 *db;
	sqlite3_stmt *stmt[VARDB_NUM_STATEMENTS];
};

int vardb_init(struct vardb *v);
int vardb_close(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value);
int vardb_append(struct vardb *v, const char *var, const char *value);
int vardb_dump(struct vardb *v);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_get(struct vardb *v, const char *var, int varlen, char **dest);

#endif
