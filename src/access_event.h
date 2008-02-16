#ifndef access_event_h
#define access_event_h

#include <sys/param.h>

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
 * file accesses. Note that not all of the bytes in the file array are sent -
 * only up to (and including) the nul terminator are sent. Any changes to this
 * structure should be reflected in the access_event_size() function below.
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

	/** The filename corresponding to the event. Not needed for the
	 * ACCESS_STOP_SERVER event.
	 */
	char file[MAXPATHLEN];
};

/** Calculate the size 'on the wire' of an access_event structure. The entire
 * 'file' field is not sent for efficiency purposes. Note this just calculates
 * the entire size of the structure except the 'file' field, and then adds in
 * the length of the file string including the nul-terminator.
 */
static inline int access_event_size(struct access_event *e)
{
	return (sizeof(struct access_event) - sizeof(e->file)) +
		strlen(e->file) + 1;
}

#endif
