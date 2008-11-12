#ifndef tup_tupid_h
#define tup_tupid_h

#include <sqlite3.h>

/* Environment variable passed to updater programs to know where to write
 * links to/from.
 */
#define TUP_CMD_ID "TUP_CMD_ID"

typedef sqlite3_int64 tupid_t;

#endif
