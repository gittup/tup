/* _ATFILE_SOURCE for fstatat() */
#define _ATFILE_SOURCE
#include "path.h"
#include "flist.h"
#include "fileio.h"
#include "monitor.h"
#include "db.h"
#include "config.h"
#include "entry.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int watch_path(tupid_t dt, int dfd, const char *file, struct tupid_entries *root,
	       int (*callback)(tupid_t newdt, int dfd, const char *file))
{
	struct flist f = {0, 0, 0};
	struct stat buf;
	tupid_t newdt;

	if(fstatat(dfd, file, &buf, AT_SYMLINK_NOFOLLOW) != 0) {
		if(errno == ENOENT) {
			/* The file may have been created and then removed before
			 * we got here. Assume the file is now gone (t7037).
			 */
			return 0;
		} else {
			fprintf(stderr, "tup monitor error: fstatat failed\n");
			perror(file);
			return -1;
		}
	}

	if(S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode)) {
		tupid_t tupid;
		tupid = tup_file_mod_mtime(dt, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(root) {
			tupid_tree_remove(root, tupid);
		}
		return 0;
	} else if(S_ISDIR(buf.st_mode)) {
		int newfd;
		struct tup_entry *gitignore_tent = NULL;
		int gitignore_found = 0;

		newdt = create_dir_file(dt, file);
		if(root) {
			tupid_tree_remove(root, newdt);
		}

		if(callback) {
			if(callback(newdt, dfd, file) < 0)
				return -1;
		}

		if(tup_db_select_tent(newdt, ".gitignore", &gitignore_tent) < 0)
			return -1;

		newfd = openat(dfd, file, O_RDONLY);
		if(newfd < 0) {
			fprintf(stderr, "tup monitor error: Unable to openat() directory.\n");
			perror(file);
			return -1;
		}
		if(fchdir(newfd) < 0) {
			perror("fchdir");
			return -1;
		}

		flist_foreach(&f, ".") {
			if(f.filename[0] == '.') {
				if(strcmp(f.filename, ".gitignore") == 0)
					gitignore_found = 1;
				continue;
			}
			if(watch_path(newdt, newfd, f.filename, root,
				      callback) < 0)
				return -1;
		}
		close(newfd);

		/* If we should have a .gitignore file but it was removed for
		 * some reason, re-parse the current Tupfile so it gets
		 * created again (t2077).
		 */
		if(gitignore_tent && !gitignore_found) {
			if(tup_db_add_create_list(newdt) < 0)
				return -1;
		}
		return 0;
	} else {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			file);
		return -1;
	}
}

int tup_scan(void)
{
	struct tupid_entries scan_root = {NULL};
	if(tup_db_scan_begin(&scan_root) < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", &scan_root, NULL) < 0)
		return -1;
	if(tup_db_scan_end(&scan_root) < 0)
		return -1;
	return 0;
}
