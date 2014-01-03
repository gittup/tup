/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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

#include "dirpath.h"
#include "compat/dir_mutex.h"
#include "tup/tupid_tree.h"
#include "tup/compat.h"
#include "tup/config.h"
#include "tup/container.h"
#include <stdio.h>

static struct tupid_entries root = {NULL};
static int dp_fd = 10000;

struct dirpath {
	struct tupid_tree tnode;
	char *path;
};

const char *win32_get_dirpath(int dfd)
{
	struct tupid_tree *tt;

	if(dfd == tup_top_fd()) {
		return get_tup_top();
	}
	pthread_mutex_lock(&dir_mutex);
	tt = tupid_tree_search(&root, dfd);
	pthread_mutex_unlock(&dir_mutex);
	if(tt) {
		struct dirpath *dp = container_of(tt, struct dirpath, tnode);
		return dp->path;
	}
	return NULL;
}

int win32_add_dirpath(const char *path)
{
	struct dirpath *dp;
	char buf[PATH_MAX];
	int len1 = 0;
	int len2;

	dp = malloc(sizeof *dp);
	if(!dp) {
		perror("malloc");
		return -1;
	}
	if(!is_full_path(path)) {
		/* Relative paths get prefixed with getcwd */
		if(getcwd(buf, sizeof(buf)) == NULL) {
			perror("getcwd");
			return -1;
		}
		len1 = strlen(buf);
	}
	len2 = strlen(path);
	dp->path = malloc(len1 + len2 + 2);
	if(!dp->path) {
		perror("malloc");
		return -1;
	}
	if(!is_full_path(path)) {
		/* Relative paths */
		memcpy(dp->path, buf, len1);
		dp->path[len1] = '\\';
		memcpy(dp->path+len1+1, path, len2);
		dp->path[len1 + len2 + 1] = 0;
	} else {
		/* Full paths */
		memcpy(dp->path, path, len2);
		dp->path[len2] = 0;
	}

	pthread_mutex_lock(&dir_mutex);
	dp->tnode.tupid = dp_fd;
	dp_fd++;

	if(tupid_tree_insert(&root, &dp->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to add dirpath for '%s'\n", path);
		goto out_err;
	}
	pthread_mutex_unlock(&dir_mutex);
	return dp->tnode.tupid;

out_err:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}

int win32_rm_dirpath(int dfd)
{
	struct tupid_tree *tt;
	int rc = 0;

	pthread_mutex_lock(&dir_mutex);
	tt = tupid_tree_search(&root, dfd);
	if(tt) {
		struct dirpath *dp = container_of(tt, struct dirpath, tnode);
		tupid_tree_rm(&root, tt);
		free(dp->path);
		free(dp);
		rc = 1;
	}
	pthread_mutex_unlock(&dir_mutex);
	return rc;
}

int win32_dup(int oldfd)
{
	struct tupid_tree *tt;
	int rc = -2;

	pthread_mutex_lock(&dir_mutex);
	tt = tupid_tree_search(&root, oldfd);
	if(tt) {
		struct dirpath *dp = container_of(tt, struct dirpath, tnode);
		struct dirpath *new;

		new = malloc(sizeof *new);
		if(!new) {
			perror("malloc");
			goto out_err;
		}
		new->path = strdup(dp->path);
		if(!new->path) {
			perror("strdup");
			goto out_err;
		}
		new->tnode.tupid = dp_fd;
		if(tupid_tree_insert(&root, &new->tnode) < 0) {
			fprintf(stderr, "tup error: Unable to dup fd %i\n", oldfd);
			goto out_err;
		}
		rc = dp_fd;
		dp_fd++;
	}
	pthread_mutex_unlock(&dir_mutex);
	return rc;

out_err:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
