#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include "tup_fuse_fs.h"
#include "tup/access_event.h"
#include "tup/config.h"
#include "tup/file.h"
#include "tup/thread_tree.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

static struct thread_root troot = THREAD_ROOT_INITIALIZER;

int tup_fuse_add_group(int pid, struct file_info *finfo)
{
	finfo->tnode.id = pid;
	if(thread_tree_insert(&troot, &finfo->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert pid %i into the fuse tree\n", pid);
		return -1;
	}
	return 0;
}

int tup_fuse_rm_group(struct file_info *finfo)
{
	thread_tree_rm(&troot, &finfo->tnode);
	return 0;
}

static struct file_info *get_finfo(void)
{
	int pgid;
	struct thread_tree *tt;
	int pid = fuse_get_context()->pid;

	pgid = getpgid(pid);
	tt = thread_tree_search(&troot, pgid);
	if(tt) {
		return container_of(tt, struct file_info, tnode);
	}
	return NULL;
}

static const char *peel(const char *path)
{
	if(strncmp(path, get_tup_top(), get_tup_top_len()) == 0) {
		path += get_tup_top_len();
		if(path[0])
			path++;
	}
	return path;
}

static void tup_fuse_handle_file(const char *path, enum access_type at)
{
	struct file_info *finfo;

	finfo = get_finfo();
	if(finfo) {
		/* TODO: Remove 1 (DOT_DT)? All fuse paths are full */
		if(handle_open_file(at, peel(path), finfo, 1) < 0) {
			/* TODO: Set failure on internal server? */
			fprintf(stderr, "tup internal error: handle open file failed\n");
			return;
		}
	}
}

/* tup_fs_* originally from fuse-2.8.5/example/fusexmp.c */
static int tup_fs_getattr(const char *path, struct stat *stbuf)
{
	int res;
	const char *peeled;

	/* Each server should put its pgid into the tree before execing the
	 * subprocess. If we don't find the current pgid in the tree, we bail
	 * since nobody else is allowed to look at our filesystem. If they could,
	 * that would hose up our dependency analysis.
	 *
	 * This check is only done here since everything starts with getattr.
	 */
	if(thread_tree_search(&troot, getpgid(fuse_get_context()->pid)) == NULL) {
		return -ENOENT;
	}

	peeled = peel(path);

	/* First we get a getattr("@tup@"), then we get a
	 * getattr("@tup@/CONFIG_FOO"). So first time we return success so fuse
	 * will assume the directory is there, then the second time we keep
	 * track of the variable and return failure because we're not actually
	 * going to open anything.
	 */
	if(strncmp(peeled, "@tup@", 5) == 0) {
		const char *var = peeled + 5;
		if(var[0] == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		} else {
			/* skip '/' */
			var++;
			tup_fuse_handle_file(var, ACCESS_VAR);
			return -1;
		}
	}

	res = lstat(path, stbuf);
	if (res == -1) {
		if(errno == ENOENT || errno == ENOTDIR) {
			tup_fuse_handle_file(peeled, ACCESS_GHOST);
		}
		return -errno;
	}
	tup_fuse_handle_file(peeled, ACCESS_READ);

	return 0;
}

static int tup_fs_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;
	tup_fuse_handle_file(path, ACCESS_READ);

	buf[res] = '\0';
	return 0;
}


static int tup_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	tup_fuse_handle_file(path, ACCESS_READ);
	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int tup_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;
	tup_fuse_handle_file(path, ACCESS_UNLINK);

	return 0;
}

static int tup_fs_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_symlink(const char *from, const char *to)
{
	int res;
	struct file_info *finfo;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	finfo = get_finfo();
	if(finfo) {
		/* TODO: 1 == DOT_DT - is this still necessary? */
		handle_file(ACCESS_SYMLINK, peel(from), peel(to), finfo, 1);
	}

	return 0;
}

static int tup_fs_rename(const char *from, const char *to)
{
	int res;
	struct file_info *finfo;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	finfo = get_finfo();
	if(finfo) {
		handle_rename(peel(from), peel(to), finfo);
	}

	return 0;
}

static int tup_fs_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_truncate(const char *path, off_t size)
{
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	enum access_type at = ACCESS_READ;

	if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
		at = ACCESS_WRITE;
	res = open(path, fi->flags);
	if(res < 0) {
		res = -errno;
		if(errno == ENOENT || errno == ENOTDIR) {
			at = ACCESS_GHOST;
		}
	}
	fi->fh = res;

	tup_fuse_handle_file(path, at);

	return 0;
}

static int tup_fs_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	int res;

	if(path) {}

	res = pread(fi->fh, buf, size, offset);
	if (res == -1)
		res = -errno;

	return res;
}

static int tup_fs_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	int res;

	if(path) {}

	res = pwrite(fi->fh, buf, size, offset);
	if (res == -1)
		res = -errno;

	return res;
}

static int tup_fs_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_release(const char *path, struct fuse_file_info *fi)
{
	if(path) {}
	close(fi->fh);
	return 0;
}

struct fuse_operations tup_fs_oper = {
	.getattr = tup_fs_getattr,
	.access = tup_fs_access,
	.readlink = tup_fs_readlink,
	.readdir = tup_fs_readdir,
	.mknod = tup_fs_mknod,
	.mkdir = tup_fs_mkdir,
	.symlink = tup_fs_symlink,
	.unlink = tup_fs_unlink,
	.rmdir = tup_fs_rmdir,
	.rename = tup_fs_rename,
	.link = tup_fs_link,
	.chmod = tup_fs_chmod,
	.chown = tup_fs_chown,
	.truncate = tup_fs_truncate,
	.utimens = tup_fs_utimens,
	.open = tup_fs_open,
	.read = tup_fs_read,
	.write = tup_fs_write,
	.statfs = tup_fs_statfs,
	.release = tup_fs_release,
};
