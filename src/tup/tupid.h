#ifndef tup_tupid_h
#define tup_tupid_h

#include <string.h>
#include <sqlite3.h>

#define SHA1_HASH_SIZE 20

/** Enough space for the sha1 hash in ascii hex */
#define SHA1_X "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SHA1_XD "xx/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SHA1_XD_SEP 2

/** Size of the tupid_t type, which is the sha1 hash written out in a hex
 * string.
 *
 * NOTE: Does *NOT* include nul-terminator. Careful when printing.
 */
#define TUPID_SIZE SHA1_HASH_SIZE * 2

/* Environment variable passed to updater programs to know where to write
 * links to/from.
 */
#define TUP_CMD_ID "TUP_CMD_ID"

typedef char tupid_t[TUPID_SIZE];
typedef sqlite3_int64 new_tupid_t;

/** Get the tupid from the filename. The tupid is *not* nul-terminated. */
void tupid_from_filename(tupid_t tupid, const char *path);
void *tupid_init(void);
void tupid_update(void *handle, const char *s);
void tupid_final(tupid_t tupid, void *handle);

static inline void tupid_to_xd(char *xd, const tupid_t tupid)
{
	memcpy(xd, tupid, SHA1_XD_SEP);
	memcpy(xd + SHA1_XD_SEP + 1, tupid + SHA1_XD_SEP,
	       sizeof(tupid_t) - SHA1_XD_SEP);
}

#endif
