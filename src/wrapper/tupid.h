#ifndef tupid_h
#define tupid_h

/** Size of the SHA1 hash */
#define SHA1_HASH_SIZE 20

/** Size of the tupid_t type, which is the sha1 hash written out in a hex
 * string (including nul-terminator)
 */
#define TUPID_SIZE SHA1_HASH_SIZE * 2 + 1

typedef char tupid_t[TUPID_SIZE];

/** Get the tupid from the filename. Result is *not* nul-terminated */
void tupid_from_filename(tupid_t tupid, const char *filename);

#endif
