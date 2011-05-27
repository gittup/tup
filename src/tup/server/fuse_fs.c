#define _ATFILE_SOURCE
#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include "tup_fuse_fs.h"
#include "tup/access_event.h"
#include "tup/config.h"
#include "tup/file.h"
#include "tup/thread_tree.h"
#include "tup/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

static struct thread_root troot = THREAD_ROOT_INITIALIZER;

int tup_fuse_add_group(int id, struct file_info *finfo)
{
	finfo->tnode.id = id;
	if(thread_tree_insert(&troot, &finfo->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert id %i into the fuse tree\n", id);
		return -1;
	}
	return 0;
}

int tup_fuse_rm_group(struct file_info *finfo)
{
	thread_tree_rm(&troot, &finfo->tnode);
	return 0;
}

#define TUP_JOB "@tupjob-"
static struct file_info *get_finfo(const char *path)
{
	struct thread_tree *tt;
	int jobnum;

	if(strncmp(path, get_tup_top(), get_tup_top_len()) != 0) {
		return NULL;
	}

	path += get_tup_top_len();
	if(!path[0]) {
		return NULL;
	}
	path++;
	if(strncmp(path, TUP_JOB, sizeof(TUP_JOB)-1) != 0) {
		return NULL;
	}

	path += sizeof(TUP_JOB)-1;
	jobnum = strtol(path, NULL, 0);
	tt = thread_tree_search(&troot, jobnum);
	if(tt) {
		struct file_info *finfo;
		finfo = container_of(tt, struct file_info, tnode);
		return finfo;
	}

	return NULL;
}

static const char *peel(const char *path)
{
	if(strncmp(path, get_tup_top(), get_tup_top_len()) == 0) {
		path += get_tup_top_len();
		if(path[0]) {
			path++;
			if(strncmp(path, TUP_JOB, sizeof(TUP_JOB-1)) == 0) {
				char *slash;
				slash = strchr(path, '/');
				if(slash) {
					path = slash + 1;
				} else {
					path = ".";
				}
			}
		} else {
			path = ".";
		}
	}
	return path;
}

static struct mapping *add_mapping(const char *path)
{
	static int filenum = 0;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	struct file_info *finfo;
	struct mapping *map = NULL;

	finfo = get_finfo(path);
	if(finfo) {
		int size;
		int myfile;

		map = malloc(sizeof *map);
		if(!map) {
			perror("malloc");
			return NULL;
		}
		map->realname = strdup(peel(path));
		if(!map->realname) {
			perror("strdup");
			return NULL;
		}
		size = sizeof(int) * 2 + sizeof(TUP_TMP) + 1;
		map->tmpname = malloc(size);
		if(!map->tmpname) {
			perror("malloc");
			return NULL;
		}
		pthread_mutex_lock(&lock);
		myfile = filenum;
		filenum++;
		pthread_mutex_unlock(&lock);

		if(snprintf(map->tmpname, size, TUP_TMP "/%x", myfile) >= size) {
			fprintf(stderr, "tup internal error: mapping tmpname is sized incorrectly.\n");
			return NULL;
		}

		list_add(&map->list, &finfo->mapping_list);
	}
	return map;
}

static struct mapping *find_mapping(const char *path)
{
	struct file_info *finfo;
	const char *peeled;

	finfo = get_finfo(path);
	if(finfo) {
		struct mapping *map;
		peeled = peel(path);
		list_for_each_entry(map, &finfo->mapping_list, list) {
			if(strcmp(peeled, map->realname) == 0) {
				return map;
			}
		}
	}
	return NULL;
}

static void tup_fuse_handle_file(const char *path, enum access_type at)
{
	struct file_info *finfo;

	finfo = get_finfo(path);
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
	struct mapping *map;
	struct tmpdir *tmpdir;
	struct file_info *finfo;

	/* Only processes spawned by tup should be able to access our
	 * file-system. This is determined by the fact that all sub-processes
	 * should be in the same process group as tup itself. Since the fuse
	 * thread runs in the main tup process, we can check our own pgid by
	 * using getpgid(0). If their pgid doesn't match, we bail since nobody
	 * else is allowed to look at our filesystem. If they could, that would
	 * hose up our dependency analysis.
	 *
	 * This check is only done here since everything starts with getattr.
	 */
	if(getpgid(0) != getpgid(fuse_get_context()->pid)) {
		if(server_debug_enabled()) {
			fprintf(stderr, "[33mtup fuse warning: Process pid=%i, uid=%i, gid=%i is trying to access the tup server's fuse filesystem.[0m\n",
					fuse_get_context()->pid, fuse_get_context()->uid, fuse_get_context()->gid);
		}
		return -EPERM;
	}

	peeled = peel(path);

	/* If we have a temporary directory of the name we're trying to do
	 * getattr(), just pretend it has the same permissions as the top
	 * tup directory. This isn't necessarily accurate, but should work for
	 * most cases.
	 */
	finfo = get_finfo(path);
	if(finfo) {
		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				if(fstat(tup_top_fd(), stbuf) < 0)
					return -errno;
				return 0;
			}
		}
	}

	map = find_mapping(path);
	if(map)
		peeled = map->tmpname;

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

			if(finfo) {
				/* TODO: 1 is always top */
				if(handle_open_file(ACCESS_VAR, var, finfo, 1) < 0) {
					fprintf(stderr, "tup error: Unable to save dependency on @-%s\n", var);
					return 1;
				}
			}
			/* Always return error, since we can't actually open
			 * an @-variable.
			 */
			return -1;
		}
	}

	res = lstat(peeled, stbuf);
	if (res == -1) {
		if(errno == ENOENT || errno == ENOTDIR) {
			tup_fuse_handle_file(path, ACCESS_GHOST);
		}
		return -errno;
	}
	tup_fuse_handle_file(path, ACCESS_READ);

	return 0;
}

static int tup_fs_access(const char *path, int mask)
{
	int res;
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);

	map = find_mapping(path);
	if(map)
		peeled = map->tmpname;

	/* This is preceded by a getattr - no need to handle a read event */
	res = access(peeled, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_readlink(const char *path, char *buf, size_t size)
{
	int res;
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);

	map = find_mapping(path);
	if(map)
		peeled = map->tmpname;

	res = readlink(peeled, buf, size - 1);
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
	const char *peeled;
	struct file_info *finfo;
	int is_tmpdir = 0;

	(void) offset;
	(void) fi;

	peeled = peel(path);
	finfo = get_finfo(path);
	if(finfo) {
		struct tmpdir *tmpdir;
		struct mapping *map;
		struct stat st;

		/* If we are doing readdir() on a temporary directory, make
		 * sure we don't try to save the dependency or do a real
		 * opendir(), since that won't work.
		 */
		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				is_tmpdir = 1;
				break;
			}
		}

		/* Check any mappings to see if there are extra files that
		 * we need to add to the list in addition to whatever we
		 * get from the real opendir/readdir (if applicable).
		 */
		list_for_each_entry(map, &finfo->mapping_list, list) {
			const char *realname;

			/* Get the 'real' realname of the file. Eg: sub/bar.txt
			 * becomes "bar.txt" if we are doing readdir("sub").
			 */
			if(peeled[0] == '.') {
				realname = map->realname;
			} else {
				int len;
				len = strlen(peeled);
				if(strncmp(peeled, map->realname, len) != 0)
					continue;
				if(map->realname[len] != '/')
					continue;
				realname = &map->realname[len+1];
			}
			/* Make sure we don't include "sub/dir/bar.txt" if
			 * we are just doing readdir("sub").
			 */
			if(strchr(realname, '/') != NULL)
				continue;

			if(lstat(map->tmpname, &st) < 0) {
				perror("lstat");
				fprintf(stderr, "tup error: Unable to stat temporary file '%s'\n", map->tmpname);
				return -1;
			}
			if(filler(buf, realname, &st, 0))
				break;
		}
	}

	if(is_tmpdir)
		return 0;

	tup_fuse_handle_file(path, ACCESS_READ);
	dp = opendir(peeled);
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
	if(rdev) {}

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		int rc;
		struct mapping *map;
		int flags = O_CREAT | O_EXCL | O_WRONLY;
		map = add_mapping(path);
		if(!map) {
			return -ENOMEM;
		} else {
			/* TODO: Error check */
			tup_fuse_handle_file(path, ACCESS_WRITE);

			rc = openat(tup_top_fd(), map->tmpname, flags, mode);
			if(rc < 0)
				return -errno;
			close(rc);
		}
	} else {
		/* Other things (eg: fifos, actual device nodes) are not
		 * permitted.
		 */
		fprintf(stderr, "tup error: mknod() with mode 0x%x is not permitted.\n", mode);
		return -EPERM;
	}

	return 0;
}

static int tup_fs_mkdir(const char *path, mode_t mode)
{
	struct tmpdir *tmpdir;
	struct file_info *finfo;

	if(mode) {}

	finfo = get_finfo(path);
	if(finfo) {
		tmpdir = malloc(sizeof *tmpdir);
		if(!tmpdir) {
			perror("malloc");
			return -ENOMEM;
		}
		tmpdir->dirname = strdup(peel(path));
		if(!tmpdir->dirname) {
			perror("strdup");
			return -ENOMEM;
		}

		list_add(&tmpdir->list, &finfo->tmpdir_list);
		return 0;
	}
	return -EPERM;
}

static int tup_fs_unlink(const char *path)
{
	struct mapping *map;

	map = find_mapping(path);
	if(map) {
		tup_fuse_handle_file(path, ACCESS_UNLINK);
		unlink(map->tmpname);
		del_map(map);
		return 0;
	}
	fprintf(stderr, "tup error: Unable to unlink files not created during this job.\n");
	return -EPERM;
}

static int tup_fs_rmdir(const char *path)
{
	struct tmpdir *tmpdir;
	const char *peeled;
	struct file_info *finfo;

	finfo = get_finfo(path);
	if(finfo) {
		peeled = peel(path);
		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				list_del(&tmpdir->list);
				free(tmpdir->dirname);
				free(tmpdir);
				return 0;
			}
		}
	}
	fprintf(stderr, "tup error: Unable to rmdir a directory not created during this job.\n");
	return -EPERM;
}

static int tup_fs_symlink(const char *from, const char *to)
{
	int res;
	struct file_info *finfo;
	struct mapping *tomap;

	tomap = add_mapping(to);
	if(!tomap) {
		return -ENOMEM;
	}

	res = symlink(from, tomap->tmpname);
	if (res == -1)
		return -errno;

	finfo = get_finfo(to);
	if(finfo) {
		/* TODO: 1 == DOT_DT - is this still necessary? */
		handle_file(ACCESS_SYMLINK, from, peel(to), finfo, 1);
	}

	return 0;
}

static int tup_fs_rename(const char *from, const char *to)
{
	struct file_info *finfo;
	const char *peelfrom;
	const char *peelto;
	struct mapping *map;

	peelfrom = peel(from);
	peelto = peel(to);

	/* If we are re-naming to a previously created file, then delete the
	 * old mapping. (eg: 'ar' will create an empty library, so we have one
	 * mapping, then create a new temp file and rename it over to 'ar', so
	 * we have a new mapping for the temp node. We need to delete the first
	 * empty one since that file is overwritten).
	 */
	map = find_mapping(to);
	if(map) {
		unlink(map->tmpname);
		del_map(map);
	}

	map = find_mapping(from);
	if(!map)
		return -ENOENT;

	free(map->realname);
	map->realname = strdup(peelto);
	if(!map->realname) {
		perror("strdup");
		return -ENOMEM;
	}

	finfo = get_finfo(to);
	if(finfo) {
		handle_rename(peelfrom, peelto, finfo);
	}

	return 0;
}

static int tup_fs_link(const char *from, const char *to)
{
	if(from || to) {}

	fprintf(stderr, "tup error: hard links are not supported.\n");
	return -EPERM;
}

static int tup_fs_chmod(const char *path, mode_t mode)
{
	struct mapping *map;

	map = find_mapping(path);
	if(map) {
		if(chmod(map->tmpname, mode) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to chmod() files not created by this job.\n");
	return -EPERM;
}

static int tup_fs_chown(const char *path, uid_t uid, gid_t gid)
{
	struct mapping *map;

	map = find_mapping(path);
	if(map) {
		if(lchown(map->tmpname, uid, gid) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to chown() files not created by this job.\n");
	return -EPERM;
}

static int tup_fs_truncate(const char *path, off_t size)
{
	struct mapping *map;

	/* TODO: error check? */
	tup_fuse_handle_file(path, ACCESS_WRITE);
	map = find_mapping(path);
	if(map) {
		if(truncate(map->tmpname, size) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to truncate() files not created by this job.\n");
	return -EPERM;
}

static int tup_fs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);
	map = find_mapping(path);
	if(map) {
		peeled = map->tmpname;

		tv[0].tv_sec = ts[0].tv_sec;
		tv[0].tv_usec = ts[0].tv_nsec / 1000;
		tv[1].tv_sec = ts[1].tv_sec;
		tv[1].tv_usec = ts[1].tv_nsec / 1000;

		res = utimes(peeled, tv);
		if (res == -1)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to utimens() files not created by this job.\n");
	return -EPERM;
}

static int tup_fs_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	enum access_type at = ACCESS_READ;
	const char *peeled;
	const char *openfile;
	struct mapping *map;

	peeled = peel(path);
	map = find_mapping(path);
	if(map) {
		openfile = map->tmpname;
	} else {
		openfile = peeled;
	}

	if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
		at = ACCESS_WRITE;
	res = open(openfile, fi->flags);
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
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);

	map = find_mapping(path);
	if(map)
		peeled = map->tmpname;

	res = statvfs(peeled, stbuf);
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
