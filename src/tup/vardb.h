#ifndef tup_vardb_h
#define tup_vardb_h

#include <sqlite3.h>

struct vardb {
	sqlite3 *db;
};

int vardb_init(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value);
int vardb_append(struct vardb *v, const char *var, const char *value);
int vardb_dump(struct vardb *v);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_get(struct vardb *v, const char *var, int varlen, char **dest);

#endif
