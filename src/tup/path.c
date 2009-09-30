/* _ATFILE_SOURCE for fstatat() */
#define _ATFILE_SOURCE
/* _GNU_SOURCE for fdopendir */
#define _GNU_SOURCE
#include "path.h"
#include "flist.h"
#include "fileio.h"
#include "monitor.h"
#include "db.h"
#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int watch_path(tupid_t dt, int dfd, const char *file, int tmp_list,
	       int (*callback)(tupid_t newdt, int dfd, const char *file))
{
	struct flist f = {0, 0, 0};
	struct stat buf;
	tupid_t newdt;

	if(fstatat(dfd, file, &buf, AT_SYMLINK_NOFOLLOW) != 0) {
		fprintf(stderr, "tup monitor error: fstatat failed\n");
		perror(file);
		return -1;
	}

	if(S_ISREG(buf.st_mode)) {
		tupid_t tupid;
		tupid = tup_file_mod_mtime(dt, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(tmp_list) {
			if(tup_db_unflag_tmp(tupid) < 0)
				return -1;
		}
		return 0;
	} else if(S_ISLNK(buf.st_mode)) {
		tupid_t tupid;

		tupid = update_symlink_fileat(dt, dfd, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(tmp_list) {
			if(tup_db_unflag_tmp(tupid) < 0)
				return -1;
		}
		return 0;
	} else if(S_ISDIR(buf.st_mode)) {
		int newfd;
		int flistfd;

		newdt = create_dir_file(dt, file);
		if(tmp_list) {
			if(tup_db_unflag_tmp(newdt) < 0)
				return -1;
		}

		if(callback)
			callback(newdt, dfd, file);

		newfd = openat(dfd, file, O_RDONLY);
		if(newfd < 0) {
			fprintf(stderr, "tup monitor error: Unable to openat() file.\n");
			perror(file);
			return -1;
		}
		flistfd = dup(newfd);
		if(flistfd < 0) {
			fprintf(stderr, "tup monitor error: Unable to dup file descriptor.\n");
			perror("dup");
			return -1;
		}

		/* flist_foreachfd uses fdopendir, which takes ownership of
		 * flistfd and closes it for us. That's why it's dup'd and only
		 * newfd is closed explicitly.
		 */
		flist_foreachfd(&f, flistfd) {
			if(f.filename[0] == '.')
				continue;
			if(watch_path(newdt, newfd, f.filename, tmp_list,
				      callback) < 0)
				return -1;
		}
		close(newfd);
		return 0;
	} else {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			file);
		return -1;
	}
}

int tup_scan(void)
{
	if(tup_db_scan_begin() < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", 1, NULL) < 0)
		return -1;
	if(tup_db_scan_end() < 0)
		return -1;
	return 0;
}
