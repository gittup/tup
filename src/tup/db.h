#ifndef tup_db_h
#define tup_db_h

#include <sqlite3.h>

#define TUP_DB_FILE ".tup/db"

extern sqlite3 *tup_db;

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
	TUP_NODE_ROOT,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY=1,
	TUP_FLAGS_CREATE=2,
	TUP_FLAGS_DELETE=4,
};

int tup_open_db(void);
int tup_create_db(void);
int tup_db_exec(const char *sql, ...);
int tup_db_select(int (*callback)(void *, int, char **, char **), void *arg,
		  const char *sql, ...);

#endif
