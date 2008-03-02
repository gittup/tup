#ifndef tupid_h
#define tupid_h

/** Size of the SHA1 hash */
#define SHA1_HASH_SIZE 20

/** Enough space for the sha1 hash in ascii hex */
#define SHA1_X "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

/** SHA1 hash of ".tup" */
#define TUPDIR_HASH "9692fbc94cb1bc4ed7650fc86f47d0f73436bcf6"

/** Size of the tupid_t type, which is the sha1 hash written out in a hex
 * string.
 *
 * NOTE: Does *NOT* include nul-terminator. Careful when printing.
 */
#define TUPID_SIZE SHA1_HASH_SIZE * 2

typedef char tupid_t[TUPID_SIZE];

/** Get the tupid from the filename. Result is *not* nul-terminated. Also
 * returns the true start of filename, since it may have been bumped up to
 * remove the "./". The actual filename array is not modified, of course.
 */
const char *tupid_from_filename(tupid_t tupid, const char *filename);

/** Same as above, only the path and filename are separate arguments and are
 * essentially concatenated, though without the use of a temporary buffer to
 * actually do that. The return value is the true start of the path argument.
 * The filename is assumed to not have "./" in front.
 */
const char *tupid_from_path_filename(tupid_t tupid, const char *path,
				     const char *filename);

#endif
