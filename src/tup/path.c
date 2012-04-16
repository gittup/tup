/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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
			fprintf(stderr, "tup error: fstatat failed\n");
			perror(file);
			return -1;
		}
	}

	if(S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode)) {
		tupid_t tupid;
		tupid = tup_file_mod_mtime(dt, file, buf.MTIME, 0, 0, NULL);
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

		if(tup_entry_find_name_in_dir(newdt, ".gitignore", -1, &gitignore_tent) < 0)
			return -1;
		if(gitignore_tent) {
			/* Gitignore's are never put in the modify list. We are
			 * just checking here if we have the gitignore entry in
			 * the database, meaning tup is supposed to generate the
			 * .gitignore file.
			 */
			tupid_tree_remove(root, gitignore_tent->tnode.tupid);
		}

		newfd = openat(dfd, file, O_RDONLY);
		if(newfd < 0) {
			fprintf(stderr, "tup error: Unable to openat() directory.\n");
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
		if(close(newfd) < 0) {
			perror("close(newfd)");
			return -1;
		}

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
		fprintf(stderr, "tup error: File '%s' is not regular nor a dir?\n",
			file);
		return -1;
	}
}


static int full_scan_cb(void *arg, struct tup_entry *tent, int style)
{
	struct tup_entry_head *head = arg;
	if(style) {}
	tup_entry_list_add(tent, head);
	return 0;
}

static int full_scan_dir(struct tup_entry_head *head, int dfd, tupid_t dt)
{
	struct tup_entry *tent;

	/* This is kinda tricky. We start with a dfd (for "/"), and it's tupid. Then we get add the
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
	LIST_FOREACH(tent, head, list) {
		int new_dfd = -1;
		time_t mtime = -1;
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
				mtime = 0;
			} else {
				if(fstatat(dfd, tent->name.s, &buf, AT_SYMLINK_NOFOLLOW) == 0) {
					if(S_ISDIR(buf.st_mode)) {
						mtime = 0;
						new_dfd = openat(dfd, tent->name.s, O_RDONLY);
						/* If we fail to open, new_dfd is -1 which means any
						 * future nodes are assumed to be un-openable as well.
						 */
					} else {
						mtime = buf.MTIME;
					}
				}
			}
		}

		if(mtime != tent->mtime) {
			printf("External file has changed: ");
			print_tup_entry(stdout, tent);
			printf("\n");
			/* Mark the commands as modify rather than the ghost node, since we don't
			 * expect a ghost to have flags set.
			 */
			if(tup_db_modify_cmds_by_input(tent->tnode.tupid) < 0)
				return -1;
			if(tup_db_set_mtime(tent, mtime) < 0)
				return -1;
		}

		if(full_scan_dir(head, new_dfd, tent->tnode.tupid) < 0)
			return -1;
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
	struct tup_entry_head *head;

	if(!tup_option_get_int("updater.full_deps")) {
		return 0;
	}

	dt = slash_dt();
	dfd = open("/", O_RDONLY);
	if(dfd < 0) {
		perror("/");
		fprintf(stderr, "tup error: Unable to open root directory entry for scanning full dependencies.\n");
		return -1;
	}
	head = tup_entry_get_list();
	rc = full_scan_dir(head, dfd, dt);
	tup_entry_release_list();

	if(close(dfd) < 0) {
		perror("close(/)");
		return -1;
	}
	return rc;
}

int tup_scan(void)
{
	struct tupid_entries scan_root = {NULL};
	if(tup_db_scan_begin(&scan_root) < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", &scan_root, NULL) < 0)
		return -1;
	if(scan_full_deps() < 0)
		return -1;
	if(tup_db_scan_end(&scan_root) < 0)
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
