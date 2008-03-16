#define _ATFILE_SOURCE
#include "fileio.h"
#include "flist.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int delete_name_file(const tupid_t tupid)
{
	int fd;
	int nb;
	struct flist f;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";
	char depfilename[] = ".tup/object/" SHA1_XD "/.secondary";
	static char filename[PATH_MAX];

	DEBUGP("delete name file: %.*s\n", 8, tupid);
	tupid_to_xd(tupfilename + 12, tupid);
	tupid_to_xd(depfilename + 12, tupid);

	fd = open(tupfilename, O_RDONLY);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	nb = read(fd, filename, sizeof(filename));
	if(nb < 0) {
		perror("read");
		close(fd);
		return -1;
	}
	filename[nb-1] = 0;
	close(fd);

	if(delete_if_exists(filename) < 0)
		return -1;
	if(delete_if_exists(tupfilename) < 0)
		return -1;
	if(delete_if_exists(depfilename) < 0)
		return -1;

	/* Change last / to nul to get dir name (13 accounts for '/' in
	 * SHA1_XD)
	 */
	tupfilename[13 + sizeof(tupid_t)] = 0;
	flist_foreach(&f, tupfilename) {
		if(f.filename[0] != '.') {
			DEBUGP("  move object %.*s to delete\n", 8, f.filename);
			if(create_tup_file_tupid("delete", f.filename) < 0)
				return -1;
			if(unlinkat(f.dirfd, f.filename, 0) < 0) {
				perror(f.filename);
				return -1;
			}
		}
	}
	if(rmdir(tupfilename) < 0) {
		perror(tupfilename);
		return -1;
	}
	return 0;
}
