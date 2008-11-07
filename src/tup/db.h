#ifndef tup_db_h
#define tup_db_h

#include <sqlite3.h>

#define TUP_DB_FILE ".tup/db"

extern sqlite3 *tup_db;

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY,
	TUP_FLAGS_CREATE,
	TUP_FLAGS_DELETE,
};

int tup_open_db(void);
int tup_create_db(void);

/** Not thread safe */
int tup_db_exec(char **errmsg, const char *sql, ...);

#endif
