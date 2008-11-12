#ifndef tup_access_event_h
#define tup_access_event_h

#include "tupid.h"

/** The environment variable used to pass the name of the UNIX socket server
 * to subprocesses.
 */
#define SERVER_NAME "tup_master"

enum access_type {
	ACCESS_READ,
	ACCESS_WRITE,
	ACCESS_RENAME_FROM,
	ACCESS_RENAME_TO,
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

	/** This field is used in ACCESS_RENAME_* events, and can be set to 0
	 * otherwise. It is used to match a ACCESS_RENAME_FROM event to a
	 * ACCESS_RENAME_TO event. Since a sub-process will send these events
	 * atomically, it uses its process ID as the unique identifier. Since
	 * these events may not be atomic wrt other sub-processes, this
	 * identifier is required for these two events.
	 */
	int pid;

	/** The tupid corresponding to this event. Not needed for the
	 * ACCESS_STOP_SERVER event.
	 */
	tupid_t tupid;
};

#endif
