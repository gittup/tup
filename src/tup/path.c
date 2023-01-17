/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* _ATFILE_SOURCE for fstatat() */
#define _ATFILE_SOURCE
#include "path.h"
#include "flist.h"
#include "fileio.h"
#include "db.h"
#include "config.h"
#include "compat.h"
#include "entry.h"
#include "option.h"
#include "pel_group.h"
#include "logging.h"
#include "tent_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static int watch_path_internal(tupid_t dt, const char *file,
			       int (*callback)(tupid_t newdt, const char *file, int *skip))
{
	struct flist f = FLIST_INITIALIZER;
	struct stat buf;

	if(lstat(file, &buf) != 0) {
		if(errno == ENOENT) {
			/* The file may have been created and then removed before
			 * we got here. Assume the file is now gone (t7037).
			 */
			return 0;
		} else {
			fprintf(stderr, "tup error: lstat failed\n");
			perror(file);
			return -1;
		}
	}

	if(S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode)) {
		tupid_t tupid;
		tupid = tup_file_mod_mtime(dt, file, MTIME(buf), 0, 0, NULL);
		if(tupid < 0)
			return -1;
		return 0;
	} else if(S_ISDIR(buf.st_mode)) {
		struct tupid_entries root = {NULL};
		struct tup_entry *tent;
		struct tup_entry *dtent;
		int skip = 0;

		if(dt == 0) {
			if(tup_entry_add(DOT_DT, &tent) < 0)
				return -1;
		} else {
			if(tup_entry_add(dt, &dtent) < 0)
				return -1;
			tent = tup_db_create_node(dtent, file, TUP_NODE_DIR);
			if(!tent)
				return -1;
		}

		if(callback) {
			if(callback(tent->tnode.tupid, file, &skip) < 0)
				return -1;
		}
		if(skip) {
			/* It's possible the directory was deleted in between
			 * our lstat() call and the inotify_add_watch() call
			 * in the monitor. If so just delete the node and
			 * continue (t7037 race condition).
			 */
			if(tup_file_missing(tent) < 0)
				return -1;
			return 0;
		}

		if(chdir(file) < 0) {
			if(errno == ENOENT) {
				/* It's possible that we successfully did an
				 * lstat() and inotify_add_watch(), but now
				 * failed to open the directory because it has
				 * been removed. This is also ok (t7037 race
				 * condition).
				 */
				if(tup_file_missing(tent) < 0)
					return -1;
				return 0;
			}
			fprintf(stderr, "tup error: Unable to chdir() directory.\n");
			perror(file);
			return -1;
		}

		if(tup_entry_get_dir_tree(tent, &root) < 0)
			return -1;

		flist_foreach(&f, ".") {
			struct tup_entry *subtent;

			if(tup_entry_find_name_in_dir(tent, f.filename, -1, &subtent) < 0)
				return -1;
			if(subtent) {
				tupid_tree_remove(&root, subtent->tnode.tupid);
			}
			if(f.filename[0] == '.') {
				if(pel_ignored(f.filename, -1))
					continue;
			}
			if(watch_path_internal(tent->tnode.tupid, f.filename, callback) < 0)
				return -1;
		}
		if(chdir("..") < 0) {
			perror("..");
			fprintf(stderr, "tup error: Unable to chdir() back to parent directory in watch_path()\n");
		}

		{
			struct tupid_tree *tt;
			while((tt = RB_ROOT(&root)) != NULL) {
				struct tup_entry *subtent;

				subtent = tup_entry_get(tt->tupid);
				if(tup_file_missing(subtent) < 0)
					return -1;
				tupid_tree_rm(&root, tt);
				free(tt);
			}
		}

		return 0;
	} else {
		/* Ignore block devices, fifofs, and sockets */
		return 0;
	}
}

int watch_path(tupid_t dt, const char *file,
	       int (*callback)(tupid_t newdt, const char *file, int *skip))
{
	int rc;
	rc = watch_path_internal(dt, file, callback);
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}
	return rc;
}

static int full_scan_cb(void *arg, struct tup_entry *tent)
{
	struct tent_list_head *head = arg;
	if(tent_list_add_head(head, tent) < 0)
		return -1;
	return 0;
}

static int full_scan_dir(struct tent_list_head *head, int dfd, tupid_t dt)
{
	struct tent_list *tl;

	/* This is kinda tricky. We start with a dfd (for "/"), and its tupid. Then we add the
	 * tup entries for the current dt to the front of the tup_entry list. We only use one
	 * list for the whole scan, and when we hit a dt that isn't ours that means we're done
	 * a single level of the directory. We keep our dfd open until the whole subtree is
	 * checked. If at any point we stop finding directories, dfd goes to -1 and we don't
	 * stat anymore. All missing files, or those with dfd==-1 have mtime set to -1. If the
	 * mtime differs from what we have saved, we flag that as a modification. Directories get
	 * an mtime of 0, so we can distinguish between a real directory and a ghost node. If a
	 * command tries to read from '/tmp/foo/bar', but the directory 'foo' doesn't exist yet
	 * then we get a dependency on /tmp/foo. If the directory is later created we need to know
	 * to re-execute since it may now have a 'bar' file.
	 */
	if(tup_db_select_node_dir(full_scan_cb, head, dt) < 0)
		return -1;
	tent_list_foreach(tl, head) {
		struct tup_entry *tent = tl->tent;
		int new_dfd = -1;
		struct timespec mtime = INVALID_MTIME;
		int scan_subdir = 0;

		if(tent->dt != dt)
			return 0;

		if(dfd != -1) {
			struct stat buf;

			if(is_full_path(tent->name.s)) {
				/* This is for Windows, since we store the C:
				 * or D: as the first path element. If it's one
				 * of these we can't fstatat it or openat it,
				 * so just open the new one and be on our way.
				 */
				new_dfd = open(tent->name.s, O_RDONLY);
				mtime = EXTERNAL_DIRECTORY_MTIME;
				scan_subdir = 1;
			} else {
				if(fstatat(dfd, tent->name.s, &buf, AT_SYMLINK_NOFOLLOW) == 0) {
					int link_to_dir = 0;

					if(S_ISDIR(buf.st_mode)) {
						mtime = EXTERNAL_DIRECTORY_MTIME;
					} else {
						if(S_ISLNK(buf.st_mode)) {
							/* If we have an external
							 * symlink, we want to keep
							 * going down the tree as if it
							 * were a directory. Otherwise,
							 * we use the mtime of the
							 * symlink itself as if it were
							 * a file.
							 */
							struct stat lnkbuf;
							if(fstatat(dfd, tent->name.s, &lnkbuf, 0) == 0) {
								if(S_ISDIR(lnkbuf.st_mode)) {
									link_to_dir = 1;
								}
							}
						}
						mtime = MTIME(buf);
					}

					if(S_ISDIR(buf.st_mode) || link_to_dir) {
						/* If we fail to open, new_dfd is -1 which means any
						 * future nodes are assumed to be un-openable as well.
						 */
						new_dfd = openat(dfd, tent->name.s, O_RDONLY);
						scan_subdir = 1;
					}
				}
			}
		}

		if(!MTIME_EQ(tent->mtime, mtime)) {
			log_debug_tent("Update external", tent, ", oldmtime=%li, newmtime=%li\n", tent->mtime, mtime);

			scan_subdir = 1;
			/* Mark the commands as modify rather than the ghost node, since we don't
			 * expect a ghost to have flags set.
			 */
			if(tup_db_modify_cmds_by_input(tent->tnode.tupid) < 0)
				return -1;
			/* If the file was included from io.open() in a
			 * Tupfile.lua, set dependent directory flags.
			 */
			if(tup_db_set_dependent_flags(tent->tnode.tupid) < 0)
				return -1;
			if(tup_db_set_mtime(tent, mtime) < 0)
				return -1;
		}

		if(scan_subdir) {
			if(full_scan_dir(head, new_dfd, tent->tnode.tupid) < 0)
				return -1;
		}
		if(new_dfd != -1) {
			if(close(new_dfd) < 0) {
				perror("close(new_dfd)");
				return -1;
			}
		}
	}
	return 0;
}

static int scan_full_deps(void)
{
	tupid_t dt;
	int dfd;
	int rc;
	struct tent_list_head scan_list;

	if(!tup_option_get_flag("updater.full_deps")) {
		return 0;
	}
	tent_list_init(&scan_list);

	dt = slash_dt();
	dfd = open("/", O_RDONLY);
	if(dfd < 0) {
		perror("/");
		fprintf(stderr, "tup error: Unable to open root directory entry for scanning full dependencies.\n");
		return -1;
	}
	rc = full_scan_dir(&scan_list, dfd, dt);
	free_tent_list(&scan_list);

	if(close(dfd) < 0) {
		perror("close(/)");
		return -1;
	}
	return rc;
}

int tup_scan(void)
{
	if(tup_db_scan_begin() < 0)
		return -1;
	if(watch_path(0, ".", NULL) < 0)
		return -1;
	if(scan_full_deps() < 0)
		return -1;
	if(tup_db_scan_end() < 0)
		return -1;
	return 0;
}

int tup_external_scan(void)
{
	if(tup_db_begin() < 0)
		return -1;
	if(scan_full_deps() < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}
