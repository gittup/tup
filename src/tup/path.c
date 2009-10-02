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
#include "tupid_tree.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int watch_path(tupid_t dt, int dfd, const char *file, struct rb_root *tree,
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
		if(tree) {
			tree_entry_remove(tree, tupid, NULL);
		}
		return 0;
	} else if(S_ISLNK(buf.st_mode)) {
		tupid_t tupid;

		tupid = update_symlink_fileat(dt, dfd, file, buf.st_mtime, 0);
		if(tupid < 0)
			return -1;
		if(tree) {
			tree_entry_remove(tree, tupid, NULL);
		}
		return 0;
	} else if(S_ISDIR(buf.st_mode)) {
		int newfd;
		int flistfd;

		newdt = create_dir_file(dt, file);
		if(tree) {
			tree_entry_remove(tree, newdt, NULL);
		}

		if(callback) {
			if(callback(newdt, dfd, file) < 0)
				return -1;
		}

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
			if(watch_path(newdt, newfd, f.filename, tree,
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
	struct rb_root scan_tree = RB_ROOT;
	if(tup_db_scan_begin(&scan_tree) < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", &scan_tree, NULL) < 0)
		return -1;
	if(tup_db_scan_end(&scan_tree) < 0)
		return -1;
	return 0;
}
