#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "entry.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int delete_name_file(tupid_t tupid)
{
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid) < 0)
		return -1;
	return 0;
}

int delete_file(tupid_t dt, const char *name)
{
	int dirfd;
	int rc = 0;

	dirfd = tup_entry_open_tupid(dt);
	if(dirfd < 0) {
		if(dirfd == -ENOENT) {
			/* If the directory doesn't exist, the file can't
			 * either
			 */
			return 0;
		} else {
			return -1;
		}
	}

	if(unlinkat(dirfd, name, 0) < 0) {
		/* Don't care if the file is already gone, or if the name
		 * is too long to exist in the filesystem anyway.
		 */
		if(errno != ENOENT && errno != ENAMETOOLONG) {
			perror(name);
			rc = -1;
			goto out;
		}
	}

out:
	close(dirfd);
	return rc;
}
