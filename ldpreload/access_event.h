#ifndef access_event_h
#define access_event_h

#include <sys/param.h>

#define SERVER_NAME "tup_master"

enum rw_type {
	READ_FILE,
	WRITE_FILE,
	STOP_SERVER,
};

struct access_event {
	enum rw_type rw;
	char filename[MAXPATHLEN];
};

#endif
