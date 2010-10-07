#include "dirpath.h"
#include "compat/dir_mutex.h"
#include "tup/tupid_tree.h"
#include "tup/compat.h"
#include "tup/config.h"

static struct rb_root root = RB_ROOT;
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
	int len1;
	int len2;

	dp = malloc(sizeof *dp);
	if(!dp) {
		perror("malloc");
		return -1;
	}
	if(getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		return -1;
	}
	len1 = strlen(buf);
	len2 = strlen(path);
	dp->path = malloc(len1 + len2 + 2);
	if(!dp->path) {
		perror("malloc");
		return -1;
	}
	memcpy(dp->path, buf, len1);
	dp->path[len1] = '\\';
	memcpy(dp->path+len1+1, path, len2);
	dp->path[len1 + len2 + 1] = 0;

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

void win32_rm_dirpath(int dfd)
{
	struct tupid_tree *tt;
	pthread_mutex_lock(&dir_mutex);
	tt = tupid_tree_search(&root, dfd);
	if(tt) {
		struct dirpath *dp = container_of(tt, struct dirpath, tnode);
		tupid_tree_rm(&root, tt);
		free(dp->path);
		free(dp);
	}
	pthread_mutex_unlock(&dir_mutex);
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
