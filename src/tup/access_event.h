#ifndef tup_access_event_h
#define tup_access_event_h

/** The environment variable used to pass the name of the UNIX socket server
 * to subprocesses.
 */
#define TUP_SERVER_NAME "tup_master"

/** The file descriptor for the variable dictionary. */
#define TUP_VARDICT_NAME "tup_vardict"

enum access_type {
	ACCESS_READ,
	ACCESS_WRITE,
	ACCESS_RENAME,
	ACCESS_UNLINK,
	ACCESS_VAR,
	ACCESS_SYMLINK,
	ACCESS_GHOST,
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

#endif
