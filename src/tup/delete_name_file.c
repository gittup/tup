#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "dirtree.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int delete_name_file(tupid_t tupid, tupid_t dt, tupid_t sym)
{
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_unflag_delete(tupid) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid, dt, sym) < 0)
		return -1;
	return 0;
}

int delete_file(tupid_t dt, const char *name)
{
	int dirfd;
	int rc = 0;

	dirfd = dirtree_open(dt);
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
		/* Don't care if the file is already gone. */
		if(errno != ENOENT) {
			perror(name);
			rc = -1;
			goto out;
		}
	}

out:
	close(dirfd);
	return rc;
}
