#ifndef tup_access_event_h
#define tup_access_event_h

#include "compat.h"

/** The file descriptor for the variable dictionary. */
#define TUP_VARDICT_NAME "tup_vardict"

/* The virtual directory used to pass @-variable dependencies from a client
 * program to the server.
 */
#define TUP_VAR_VIRTUAL_DIR "@tup@"
#define TUP_VAR_VIRTUAL_DIR_LEN (sizeof(TUP_VAR_VIRTUAL_DIR)-1)

enum access_type {
	ACCESS_READ,
	ACCESS_WRITE,
	ACCESS_RENAME,
	ACCESS_UNLINK,
	ACCESS_VAR,
	ACCESS_CHDIR,
	ACCESS_STOP_SERVER,
};

/** Structure sent across the unix socket to notify the main wrapper of any
 * file accesses.
 *
 * Also note that the wrapper server relies on only being able to send the
 * access_type set to ACCESS_STOP_SERVER and no additional data.
 */
struct access_event {
	/** This field must always be set to one of the values in the
	 * access_type enum.
	 */
	enum access_type at;

	/** Length of the path, which will follow the access_event struct */
	int len;

	/** Length of the second path, for events that require two paths */
	int len2;
};

#define ACCESS_EVENT_MAX_SIZE (PATH_MAX * 2 + sizeof(struct access_event))
void tup_send_event(const char *file, int len, const char *file2, int len2, int at);

#endif
