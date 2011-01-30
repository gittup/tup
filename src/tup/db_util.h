#ifndef tup_db_util_h
#define tup_db_util_h

#include "sqlite3/sqlite3.h"
int db_close(sqlite3 *db, sqlite3_stmt **stmts, int num);

#endif
