#ifndef tup_vardb_h
#define tup_vardb_h

#include "linux/rbtree.h"

struct vardb {
	struct rb_root tree;
};

int vardb_init(struct vardb *v);
int vardb_close(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value);
int vardb_append(struct vardb *v, const char *var, const char *value);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_copy(struct vardb *v, const char *var, int varlen, char **dest);
const char *vardb_get(struct vardb *v, const char *var);
void vardb_dump(struct vardb *v);

#endif
