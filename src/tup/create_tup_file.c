#include "fileio.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>

/* TODO: Revisit these - they're almost identical */

int create_tup_file(const char *tup, const char *path, const char *file,
		    int lock_fd)
{
	int rc;
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	if(flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
		/* tup must be running a wrapped command */
		if(errno == EWOULDBLOCK)
			return 0;
		/* or some other error occurred */
		perror("flock");
		return -1;
	}
	memcpy(filename + 5, tup, 6);
	path = tupid_from_path_filename(filename + 12, path, file);

	DEBUGP("create tup file: %s\n", filename);
	rc = create_if_not_exist(filename);
	flock(lock_fd, LOCK_UN);
	return rc;
}

int create_tup_file_tupid(const char *tup, const tupid_t tupid, int lock_fd)
{
	int rc;
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	if(flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
		/* tup must be running a wrapped command */
		if(errno == EWOULDBLOCK)
			return 0;
		/* or some other error occurred */
		perror("flock");
		return -1;
	}
	memcpy(filename + 5, tup, 6);
	memcpy(filename + 12, tupid, sizeof(tupid_t));

	DEBUGP("create tup file: %s\n", filename);
	rc = create_if_not_exist(filename);
	flock(lock_fd, LOCK_UN);
	return rc;
}
