#ifndef tup_tupid_h
#define tup_tupid_h

#include <string.h>

#define SHA1_HASH_SIZE 20

/** Enough space for the sha1 hash in ascii hex */
#define SHA1_X "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SHA1_XD "xx/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SHA1_XD_SEP 2

/** SHA1 hash of ".tup" */
#define TUPDIR_HASH "9692fbc94cb1bc4ed7650fc86f47d0f73436bcf6"

/** Size of the tupid_t type, which is the sha1 hash written out in a hex
 * string.
 *
 * NOTE: Does *NOT* include nul-terminator. Careful when printing.
 */
#define TUPID_SIZE SHA1_HASH_SIZE * 2

typedef char tupid_t[TUPID_SIZE];

#define TUP_CREATE 0x001
#define TUP_DELETE 0x002
#define TUP_MODIFY 0x004

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
