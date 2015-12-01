/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2015  Mike Shal <marfey@gmail.com>
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

#define _ATFILE_SOURCE
#define _GNU_SOURCE
#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include "compat/utimensat.h"
#include "tup_fuse_fs.h"
#include "tup/config.h"
#include "tup/debug.h"
#include "tup/server.h"
#include "tup/container.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>

#if defined(__FreeBSD__)
/* FreeBSD doessn't support AT_SYMLINK_NOFOLLOW in faccessat() */
static int access_flags = 0;
#else
static int access_flags = AT_SYMLINK_NOFOLLOW;
#endif

static struct thread_root troot = THREAD_ROOT_INITIALIZER;
static int server_mode = 0;
static pid_t ourpgid;
static int max_open_files = 128;

void tup_fuse_fs_init(void)
{
	struct rlimit rlim;
	ourpgid = getpgid(0);
	if(getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
		int x;
		for(x=0; x<10; x++) {
			/* Keep doubling until we hit the real limit, whatever
			 * that is. OSX sets rlim.rlim_max to -1 for some
			 * reason, so we have no idea what the limit is.
			 */
			rlim.rlim_cur *= 2;
			if(setrlimit(RLIMIT_NOFILE, &rlim) != 0)
				break;
		}
		if(getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
			max_open_files = rlim.rlim_cur / 2;
		}
	}
}

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

void tup_fuse_set_parser_mode(int mode)
{
	server_mode = mode;
}

static int is_hidden(const char *path)
{
	if(strstr(path, "/.git") != NULL)
		return 1;
	if(strstr(path, "/.tup") != NULL)
		return 1;
	if(strstr(path, "/.hg") != NULL)
		return 1;
	return 0;
}

static struct file_info *get_finfo(const char *path)
{
	struct thread_tree *tt;
	int jobnum;

	if(!path)
		return NULL;
	if(path[0] != '/')
		return NULL;
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
		finfo_lock(finfo);
		return finfo;
	}

	return NULL;
}

static void put_finfo(struct file_info *finfo)
{
	finfo_unlock(finfo);
}

static const char *peel(const char *path)
{
	if(!path)
		return NULL;
	if(path[0] != '/')
		return NULL;

	if(strncmp(path + 1, TUP_JOB, sizeof(TUP_JOB)-1) == 0) {
		char *slash;

		path += sizeof(TUP_JOB); /* +1 and -1 cancel */
		slash = strchr(path, '/');
		if(slash) {
			path = slash;
		} else {
			path = "/";
		}

	}

	return path;
}

static const char *prefix_strip(const char *peeled, const char *variant_dir)
{
	if(strncmp(peeled, get_tup_top(), get_tup_top_len()) == 0) {
		int variant_len = strlen(variant_dir);

		peeled += get_tup_top_len();

		/* Now we need to match the variant directory. Note these
		 * always begin with a '/'.
		 */
		if(strncmp(peeled, variant_dir, variant_len) != 0)
			return NULL;
		peeled += variant_len;
		if(peeled[0] != '/')
			return NULL;
		peeled++;
		return peeled;
	}
	return NULL;
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
		const char *peeled;

		peeled = peel(path);

		if(handle_open_file(ACCESS_WRITE, peeled, finfo) < 0) {
			/* TODO: Set failure on internal server? */
			fprintf(stderr, "tup internal error: handle open file failed\n");
			return NULL;
		}

		map = malloc(sizeof *map);
		if(!map) {
			perror("malloc");
			return NULL;
		}
		map->realname = strdup(peeled);
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
		map->tent = NULL; /* This is used when saving dependencies */

		pthread_mutex_lock(&lock);
		myfile = filenum;
		filenum++;
		pthread_mutex_unlock(&lock);

		if(snprintf(map->tmpname, size, TUP_TMP "/%x", myfile) >= size) {
			fprintf(stderr, "tup internal error: mapping tmpname is sized incorrectly.\n");
			return NULL;
		}

		LIST_INSERT_HEAD(&finfo->mapping_list, map, list);
		put_finfo(finfo);
	}
	return map;
}

static struct mapping *find_mapping(struct file_info *finfo, const char *path)
{
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);
	LIST_FOREACH(map, &finfo->mapping_list, list) {
		if(strcmp(peeled, map->realname) == 0) {
			return map;
		}
	}
	return NULL;
}

static int context_check(void)
{
	pid_t pgid;

	/* Only processes spawned by tup should be able to access our
	 * file-system. This is determined by the fact that all sub-processes
	 * should be in the same process group as tup itself. Since the fuse
	 * thread runs in the main tup process, we can check our own pgid by
	 * using getpgid(0). If their pgid doesn't match, we bail since nobody
	 * else is allowed to look at our filesystem. If they could, that would
	 * hose up our dependency analysis.
	 */
	pgid = getpgid(fuse_get_context()->pid);

	/* OSX will fail to return a valid pgid for a zombie process.  However,
	 * for some reason when using 'ar' to create archives, a zombie libtool
	 * process will call 'unlink' on the .fuse_hidden file. If we ignore
	 * that check, then tup will save the .fuse_hidden file as a separate
	 * output because hidden files are ignored.
	 *
	 * Separately, Linux running in a container will have a bogus fuse
	 * context pid, so getpgid() always fails. There doesn't seem to be
	 * much we can do in this case. Fortunately, if lxc is working that
	 * probably means we're using a separate mount namespace anyway, making
	 * this check moot.
	 */
	if(pgid == -1 && errno == ESRCH) {
		return 0;
	}

	if(ourpgid != pgid) {
		if(server_debug_enabled()) {
			fprintf(stderr, "[33mtup fuse warning: Process pid=%i, uid=%i, gid=%i is trying to access the tup server's fuse filesystem.[0m\n",
					fuse_get_context()->pid, fuse_get_context()->uid, fuse_get_context()->gid);
		}
		return -1;
	}
	return 0;
}

static int ignore_file(const char *path)
{
	if(strncmp(path, "/proc/", 6) == 0)
		return 1;
	return 0;
}

static void tup_fuse_handle_file(const char *path, const char *stripped, enum access_type at)
{
	struct file_info *finfo;

	if(ignore_file(peel(path)))
		return;

	finfo = get_finfo(path);
	if(finfo) {
		if(handle_open_file(at, peel(path), finfo) < 0) {
			/* TODO: Set failure on internal server? */
			fprintf(stderr, "tup internal error: handle open file failed\n");
		}
		if(stripped) {
			if(handle_open_file(at, stripped, finfo) < 0) {
				/* TODO: Set failure on internal server? */
				fprintf(stderr, "tup internal error: handle open file failed\n");
			}
		}
		put_finfo(finfo);
	}
}

static const char *get_virtual_var(const char *peeled, const char *variant_dir)
{
	const char *stripped;

	if(variant_dir) {
		stripped = prefix_strip(peeled, variant_dir);
	} else {
		if(strncmp(peeled, get_tup_top(), get_tup_top_len()) == 0) {
			peeled += get_tup_top_len();
			if(peeled[0] != '/')
				return NULL;
			peeled++;
			stripped = peeled;
		} else {
			return NULL;
		}
	}
	if(stripped) {
		const char *var = strstr(stripped, TUP_VAR_VIRTUAL_DIR);
		if(var) {
			var += TUP_VAR_VIRTUAL_DIR_LEN;
			return var;
		}
	}
	return NULL;
}

/* tup_fs_* originally from fuse-2.8.5/example/fusexmp.c */
static int tup_fs_getattr(const char *path, struct stat *stbuf)
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct tmpdir *tmpdir;
	struct file_info *finfo;
	const char *var;
	const char *variant_dir = NULL;
	const char *stripped = NULL;
	int rc;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	/* If we have a temporary directory of the name we're trying to do
	 * getattr(), just pretend it has the same permissions as the top
	 * tup directory. This isn't necessarily accurate, but should work for
	 * most cases.
	 */
	finfo = get_finfo(path);
	if(finfo) {
		if(strcmp(peeled, ".tup/mnt") == 0 || strstr(peeled, "/.tup/mnt") != NULL) {
			/* t6056 - don't allow sub-processes to mess with our
			 * data.
			 */
			put_finfo(finfo);
			return -EPERM;
		}
		rc = 0;
		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				if(fstat(tup_top_fd(), stbuf) < 0)
					rc = -errno;
				put_finfo(finfo);
				return rc;
			}
		}
		map = find_mapping(finfo, path);
		if(map)
			peeled = map->tmpname;
		else
			variant_dir = finfo->variant_dir;
		put_finfo(finfo);
	}

	/* First we get a getattr("@tup@"), then we get a
	 * getattr("@tup@/CONFIG_FOO"). So first time we return success so fuse
	 * will assume the directory is there, then the second time we keep
	 * track of the variable and return failure because we're not actually
	 * going to open anything.
	 */
	var = get_virtual_var(peeled, variant_dir);
	if(var) {
		if(var[0] == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		} else {
			/* skip '/' */
			var++;

			if(finfo) {
				finfo_lock(finfo);
				if(handle_open_file(ACCESS_VAR, var, finfo) < 0) {
					fprintf(stderr, "tup error: Unable to save dependency on @-%s\n", var);
					return 1;
				}
				finfo_unlock(finfo);
			}
			/* Always return error, since we can't actually open
			 * an @-variable.
			 */
			return -1;
		}
	}

	res = fstatat(tup_top_fd(), peeled, stbuf, AT_SYMLINK_NOFOLLOW);
	if (res == -1 && variant_dir) {
		stripped = prefix_strip(peeled, variant_dir);
		if(stripped) {
			res = fstatat(tup_top_fd(), stripped, stbuf, AT_SYMLINK_NOFOLLOW);
		}
	}

	if (res == -1) {
		rc = -errno;
	} else {
		rc = 0;
	}
	tup_fuse_handle_file(path, stripped, ACCESS_READ);

	return rc;
}

static int tup_fs_access(const char *path, int mask)
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;
	struct tmpdir *tmpdir;
	const char *var;
	const char *variant_dir = NULL;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	finfo = get_finfo(path);
	if(finfo) {
		int entry_found = 0;
		int rc = 0;

		map = find_mapping(finfo, path);
		if(map)
			peeled = map->tmpname;
		else
			variant_dir = finfo->variant_dir;


		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				/* For a temporary directory, just use the same
				 * access permissions as the top-level directory.
				 * This could be finer grained to use the actual
				 * permissions assigned in mkdir for a temp
				 * directory.
				 */
				if(faccessat(tup_top_fd(), ".", mask, access_flags) < 0)
					rc = -errno;
				entry_found = 1;
				break;
			}
		}
		put_finfo(finfo);
		if(entry_found)
			return rc;
	}

	/* OSX will call access() on the virtual directory before calling
	 * getattr() on the variable name, so we check for that here. The
	 * var[0] == 0 check means it is just the @tup@ directory itself, and
	 * not a variable name.
	 */
	var = get_virtual_var(peeled, variant_dir);
	if(var && var[0] == 0) {
		return 0;
	}

	/* This is preceded by a getattr - no need to handle a read event */
	res = faccessat(tup_top_fd(), peeled, mask, access_flags);
	if (res == -1 && variant_dir) {
		const char *stripped = prefix_strip(peeled, variant_dir);
		if(stripped) {
			res = faccessat(tup_top_fd(), stripped, mask, access_flags);
		}
	}
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_readlink(const char *path, char *buf, size_t size)
{
	int res;
	const char *peeled;
	struct file_info *finfo;
	struct mapping *map;
	const char *variant_dir = NULL;
	const char *stripped = NULL;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	finfo = get_finfo(path);
	if(finfo) {
		variant_dir = finfo->variant_dir;
		map = find_mapping(finfo, path);
		if(map)
			peeled = map->tmpname;
		put_finfo(finfo);
	}

	/* /proc/self gets special treatment, since we want the pid of the
	 * process doing the readlink(). If we let the kernel handle it then we
	 * get the pid of this fuse process, which is obviously incorrect.
	 */
	if(strcmp(peeled, "/proc/self") == 0) {
		res = snprintf(buf, size - 1, "%i", fuse_get_context()->pid);
		if(res >= (signed)size - 1) {
			/* According to readlink(2), if the buffer is too small then the result
			 * is truncated.
			 */
			res = size - 1;
		}
	} else {
		res = readlinkat(tup_top_fd(), peeled, buf, size - 1);
		if(res == -1 && variant_dir) {
			stripped = prefix_strip(peeled, variant_dir);
			if(stripped) {
				res = readlinkat(tup_top_fd(), stripped, buf, size - 1);
			}
		}
		if(res == -1)
			return -errno;
	}
	tup_fuse_handle_file(path, stripped, ACCESS_READ);

	buf[res] = '\0';
	return 0;
}

static void add_dir_entries(DIR *dp, void *buf, fuse_fill_dir_t filler,
			    int ignore_dot_tup)
{
	struct dirent *de;
	while((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;

		if(!ignore_dot_tup || strcmp(de->d_name, ".tup") != 0)
			if(filler(buf, de->d_name, &st, 0))
				break;
	}
}

static int fill_actual_directory(const char *peeled, void *buf,
				 fuse_fill_dir_t filler, int ignore_dot_tup, const char *variant_dir)
{
	DIR *dp;
	int fd;

	fd = openat(tup_top_fd(), peeled, O_RDONLY);
	if(fd >= 0) {
		dp = fdopendir(fd);
		if(dp == NULL)
			return -errno;

		add_dir_entries(dp, buf, filler, ignore_dot_tup);
		closedir(dp);
	}

	if(variant_dir)  {
		const char *stripped = prefix_strip(peeled, variant_dir);
		if(stripped) {
			fd = openat(tup_top_fd(), stripped, O_RDONLY);
			if(fd >= 0) {
				dp = fdopendir(fd);
				if(dp == NULL)
					return -errno;
				add_dir_entries(dp, buf, filler, ignore_dot_tup);
				closedir(dp);
			}
		}
	}
	return 0;
}

static int readdir_parser(const char *path, void *buf, fuse_fill_dir_t filler, const char *variant_dir)
{
	if(strncmp(path, get_tup_top(), get_tup_top_len()) == 0) {
		if(tup_fuse_server_get_dir_entries(path + get_tup_top_len(),
						   buf, filler) < 0)
			return -EPERM;
	} else {
		/* t4052 */
		return fill_actual_directory(path, buf, filler, 0, variant_dir);
	}
	return 0;
}

static int tup_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{
	const char *peeled;
	struct file_info *finfo;
	const char *variant_dir = NULL;
	const char *stripped = NULL;
	int is_tmpdir = 0;

	(void) offset;
	(void) fi;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
	finfo = get_finfo(path);
	if(finfo) {
		struct tmpdir *tmpdir;
		struct mapping *map;
		struct stat st;

		/* In the parser, we have to look at the tup database, not
		 * the filesystem.
		 */
		if(server_mode == SERVER_PARSER_MODE) {
			int rc;
			rc = readdir_parser(peeled, buf, filler, finfo->variant_dir);
			if(rc < 0) {
				finfo->server_fail = 1;
			}
			put_finfo(finfo);
			return rc;
		}

		/* If we are doing readdir() on a temporary directory, make
		 * sure we don't try to save the dependency or do a real
		 * opendir(), since that won't work.
		 */
		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				is_tmpdir = 1;
				break;
			}
		}

		/* Check any mappings to see if there are extra files that
		 * we need to add to the list in addition to whatever we
		 * get from the real opendir/readdir (if applicable).
		 */
		LIST_FOREACH(map, &finfo->mapping_list, list) {
			const char *realname;

			/* Get the 'real' realname of the file. Eg: sub/bar.txt
			 * becomes "bar.txt" if we are doing readdir("sub").
			 */
			if(peeled[0] == '.') {
				/* TODO: ?? */
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

			if(fstatat(tup_top_fd(), map->tmpname, &st, AT_SYMLINK_NOFOLLOW) < 0) {
				perror("lstat");
				fprintf(stderr, "tup error: Unable to stat temporary file '%s'\n", map->tmpname);
				put_finfo(finfo);
				return -1;
			}
			if(filler(buf, realname, &st, 0))
				break;
		}

		/* Check any tmpdir subdirs, and add them to the list */
		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			int peeled_len;
			int tmpdir_len;

			/* if this tmpdir is a subdir of the readdir() dir */
			peeled_len = strlen(peeled);
			tmpdir_len = strlen(tmpdir->dirname);
			if (tmpdir_len > peeled_len
			    && strncmp(peeled, tmpdir->dirname, peeled_len) == 0
			    && tmpdir->dirname[peeled_len] == '/') {
				const char *realname;

				if(fstat(tup_top_fd(), &st) < 0) {
					int rc = -errno;
					fprintf(stderr, "tup error: Unable to stat .tup directory\n");
					put_finfo(finfo);
					return rc;
				}

				realname = &tmpdir->dirname[peeled_len+1];

				/* Make sure we don't include "sub/dir/bar" if
				* we are just doing readdir("sub").
				*/
				if(strchr(realname, '/') != NULL)
					continue;

				if (filler(buf, realname, &st, 0))
					break;
			}
		}

		variant_dir = finfo->variant_dir;

		put_finfo(finfo);
	}

	if(is_tmpdir)
		return 0;

	tup_fuse_handle_file(path, NULL, ACCESS_READ);

	if(variant_dir) {
		stripped = prefix_strip(peeled, variant_dir);
		if(stripped) {
			if(fill_actual_directory(stripped, buf, filler, finfo != NULL, NULL) < 0)
				return -1;
		}
	}

	/* If finfo is NULL, we're outside of tup, so we don't need to ignore
	 * any files called '.tup' in that case.
	 */
	return fill_actual_directory(peeled, buf, filler, finfo != NULL, NULL);
}

static int mknod_internal(const char *path, mode_t mode, int flags, int close_fd)
{
	int rc;
	struct mapping *map;

	if(context_check() < 0)
		return -EPERM;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		map = add_mapping(path);
		if(!map) {
			return -ENOMEM;
		} else {
			/* TODO: Error check */
			tup_fuse_handle_file(path, NULL, ACCESS_WRITE);

			rc = openat(tup_top_fd(), map->tmpname, flags, mode);
			if(rc < 0)
				return -errno;
			if(close_fd) {
				if(close(rc) < 0)
					return -errno;
				rc = 0;
			}
		}
	} else if S_ISFIFO(mode) {
		map = add_mapping(path);
		if(!map) {
			return -ENOMEM;
		} else {
			rc = mkfifo(map->tmpname, mode);
			if(rc < 0)
				return -errno;
		}
	} else if S_ISSOCK(mode) {
		map = add_mapping(path);
		if(!map) {
			return -ENOMEM;
		} else {
			rc = mknod(map->tmpname, mode, 0);
			if(rc < 0)
				return -errno;
		}
	} else {
		/* Other things (eg: actual device nodes) are not
		 * permitted.
		 */
		fprintf(stderr, "tup error: mknod() with mode 0x%x is not permitted.\n", mode);
		return -EPERM;
	}

	return rc;
}

static int tup_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	if(rdev) {}
	return mknod_internal(path, mode, O_CREAT | O_EXCL | O_WRONLY, 1);
}

static int tup_fs_mkdir(const char *path, mode_t mode)
{
	struct tmpdir *tmpdir;
	struct file_info *finfo;

	if(mode) {}

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		int rc = -1;
		tmpdir = malloc(sizeof *tmpdir);
		if(!tmpdir) {
			perror("malloc");
			rc = -ENOMEM;
		} else {
			tmpdir->dirname = strdup(peel(path));
			if(!tmpdir->dirname) {
				perror("strdup");
				rc = -ENOMEM;
			}
			if(tmpdir && tmpdir->dirname) {
				LIST_INSERT_HEAD(&finfo->tmpdir_list, tmpdir, list);
				rc = 0;
			}
		}
		put_finfo(finfo);
		return rc;
	}
	return -EPERM;
}

static int tup_fs_unlink(const char *path)
{
	struct mapping *map;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			unlinkat(tup_top_fd(), map->tmpname, 0);
			del_map(map);
			put_finfo(finfo);
			tup_fuse_handle_file(path, NULL, ACCESS_UNLINK);
			return 0;
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to unlink files not created during this job: %s\n", peel(path));
	return -EPERM;
}

static int tup_fs_rmdir(const char *path)
{
	struct tmpdir *tmpdir;
	const char *peeled;
	struct file_info *finfo;
	struct mapping *map;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		peeled = peel(path);
		size_t len = strlen(peeled);

		// Ensure that there are no subdirectories
		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strncmp(tmpdir->dirname, peeled, len) == 0 && tmpdir->dirname[len] == '/') {
				put_finfo(finfo);
				return -ENOTEMPTY;
			}
		}
		// Ensure that there are no files in the directory
		LIST_FOREACH(map, &finfo->mapping_list, list) {
			if (strncmp(map->realname, peeled, len) == 0 && map->realname[len] == '/') {
				put_finfo(finfo);
				return -ENOTEMPTY;
			}
		}

		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				LIST_REMOVE(tmpdir, list);
				free(tmpdir->dirname);
				free(tmpdir);
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to rmdir a directory not created during this job: %s\n", path);
	return -EPERM;
}

static int tup_fs_symlink(const char *from, const char *to)
{
	int res;
	struct mapping *tomap;

	if(context_check() < 0)
		return -EPERM;

	tomap = add_mapping(to);
	if(!tomap) {
		return -ENOMEM;
	}

	res = symlinkat(from, tup_top_fd(), tomap->tmpname);
	if (res == -1)
		return -errno;

	return 0;
}

static int tup_fs_rename(const char *from, const char *to)
{
	struct file_info *finfo;
	const char *peelfrom;
	const char *peelto;
	struct mapping *map;

	if(context_check() < 0)
		return -EPERM;

	peelfrom = peel(from);
	peelto = peel(to);

	finfo = get_finfo(to);
	if(finfo) {
		/* If we are re-naming to a previously created file, then
		 * delete the old mapping. (eg: 'ar' will create an empty
		 * library, so we have one mapping, then create a new temp file
		 * and rename it over to 'ar', so we have a new mapping for the
		 * temp node. We need to delete the first empty one since that
		 * file is overwritten).
		 */
		map = find_mapping(finfo, to);
		if(map) {
			unlink(map->tmpname);
			del_map(map);
		}

		map = find_mapping(finfo, from);
		if(!map) {
			put_finfo(finfo);
			return -ENOENT;
		}

		free(map->realname);
		map->realname = strdup(peelto);
		if(!map->realname) {
			perror("strdup");
			put_finfo(finfo);
			return -ENOMEM;
		}

		handle_rename(peelfrom, peelto, finfo);
		put_finfo(finfo);
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
	struct file_info *finfo;
	const char *peeled;
	struct tmpdir *tmpdir;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		int rc = 0;
		map = find_mapping(finfo, path);
		if(map) {
			if(fchmodat(tup_top_fd(), map->tmpname, mode, 0) < 0)
				rc = -errno;
			put_finfo(finfo);
			return rc;
		}
		peeled = peel(path);
		LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to chmod() files not created by this job: %s\n", path);
	return -EPERM;
}

static int tup_fs_chown(const char *path, uid_t uid, gid_t gid)
{
	struct mapping *map;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		int rc = 0;
		map = find_mapping(finfo, path);
		if(map) {
			if(fchownat(tup_top_fd(), map->tmpname, uid, gid, AT_SYMLINK_NOFOLLOW) < 0)
				rc = -errno;
			put_finfo(finfo);
			return rc;
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to chown() files not created by this job: %s\n", path);
	return -EPERM;
}

static int tup_fs_truncate(const char *path, off_t size)
{
	struct mapping *map;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	/* TODO: error check? */
	tup_fuse_handle_file(path, NULL, ACCESS_WRITE);
	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			int fd;
			int rc = 0;
			fd = openat(tup_top_fd(), map->tmpname, O_WRONLY);
			if(!fd) {
				put_finfo(finfo);
				return -errno;
			}
			if(ftruncate(fd, size) < 0)
				rc = -errno;
			if(close(fd) < 0)
				rc = -errno;
			put_finfo(finfo);
			return rc;
		}
		put_finfo(finfo);
	}
	if(is_hidden(path)) {
		const char *peeled = peel(path);
		if(truncate(peeled, size) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to truncate() files not created by this job: %s\n", path);
	return -EPERM;
}

static int tup_fs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			int rc = 0;
			peeled = map->tmpname;

			res = utimensat(tup_top_fd(), peeled, ts, AT_SYMLINK_NOFOLLOW);
			if (res == -1)
				rc = -errno;
			put_finfo(finfo);
			return rc;
		}
		put_finfo(finfo);
	}
	if(is_hidden(path)) {
		peeled = peel(path);
		if(utimensat(tup_top_fd(), peeled, ts, AT_SYMLINK_NOFOLLOW) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to utimens() files not created by this job: %s\n", path);
	return -EPERM;
}

static int tup_fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int rc;
	struct file_info *finfo;
	rc = mknod_internal(path, mode, fi->flags, 0);
	if(rc < 0)
		return -errno;
	finfo = get_finfo(path);
	if(finfo) {
		if(finfo->open_count >= max_open_files) {
			close(rc);
			fi->fh = 0;
		} else {
			fi->fh = rc;
		}
		finfo->open_count++;
		put_finfo(finfo);
	}
	return 0;
}

static int tup_fs_open(const char *path, struct fuse_file_info *fi)
{
	int res = 0;
	int fd;
	enum access_type at = ACCESS_READ;
	const char *peeled;
	const char *openfile;
	struct mapping *map;
	struct file_info *finfo;
	const char *variant_dir = NULL;
	const char *stripped = NULL;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
	openfile = peeled;

	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			openfile = map->tmpname;
		} else {
			/* Only try the base dir if it's not a mapped file */
			variant_dir = finfo->variant_dir;
		}

		if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
			at = ACCESS_WRITE;
		fd = openat(tup_top_fd(), openfile, fi->flags);
		if(fd < 0 && variant_dir) {
			stripped = prefix_strip(peeled, variant_dir);
			if(stripped) {
				fd = openat(tup_top_fd(), stripped, fi->flags);
			}
		}

		if(fd < 0) {
			res = -errno;
		} else {
			if(finfo->open_count >= max_open_files) {
				close(fd);
				fi->fh = 0;
			} else {
				fi->fh = fd;
			}
			finfo->open_count++;
		}

		put_finfo(finfo);
		tup_fuse_handle_file(path, stripped, at);
	} else {
		res = -EPERM;
	}
	return res;
}

static int tup_fs_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	int res;
	int fd;

	if(context_check() < 0)
		return -EPERM;

	if(fi->fh == 0) {
		struct file_info *finfo;
		const char *openfile;

		openfile = peel(path);
		finfo = get_finfo(path);
		if(finfo) {
			struct mapping *map;
			map = find_mapping(finfo, path);
			if(map) {
				openfile = map->tmpname;
			}
			put_finfo(finfo);
		}

		fd = openat(tup_top_fd(), openfile, O_RDONLY);
		if(fd < 0)
			return -errno;
	} else {
		fd = fi->fh;
	}

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi->fh == 0) {
		close(fd);
	}

	return res;
}

static int tup_fs_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	int res;
	int fd = -1;

	if(context_check() < 0)
		return -EPERM;

	if(fi->fh == 0) {
		struct file_info *finfo;
		finfo = get_finfo(path);
		if(finfo) {
			struct mapping *map;
			map = find_mapping(finfo, path);
			if(map) {
				fd = openat(tup_top_fd(), map->tmpname, O_WRONLY);
				if(fd < 0) {
					put_finfo(finfo);
					return -errno;
				}
			}
			put_finfo(finfo);
		}
		if(fd < 0)
			return -EPERM;
	} else {
		fd = fi->fh;
	}

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi->fh == 0) {
		close(fd);
	}

	return res;
}

static int tup_fs_statfs(const char *path, struct statvfs *stbuf)
{
	int fd;
	int rc = 0;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;
	struct tmpdir *tmpdir;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			peeled = map->tmpname;
		} else {
			LIST_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
				if(strcmp(tmpdir->dirname, peeled) == 0) {
					if(fstatvfs(tup_top_fd(), stbuf) < 0)
						rc = -errno;
					put_finfo(finfo);
					return rc;
				}
			}
		}
		put_finfo(finfo);
	}

	fd = openat(tup_top_fd(), peeled, O_RDONLY);
	if(fd < 0)
		return -errno;

	if(fstatvfs(fd, stbuf) < 0)
		rc = -errno;
	if(close(fd) < 0)
		rc = -errno;
	return rc;
}

static int tup_fs_flush(const char *path, struct fuse_file_info *fi)
{
	/* We don't actually do anything here, but without flush() sometimes
	 * the sub-process will finish before our fuse fs finishes writing out
	 * all data and calling release(). Eg, if we have a command that does
	 * 'cp bigfile.txt newbigfile.txt', where bigfile.txt contains a lot of
	 * data, then the waitpid() in master_fork returns before release() is
	 * called. We then end up stating the output file and getting a bad
	 * timestamp since data is still being written out. (The file is
	 * written correctly, but our mtime that we store in the db is already
	 * out of date).
	 */
	if(path) {}
	if(fi) {}
	return 0;
}

static int tup_fs_release(const char *path, struct fuse_file_info *fi)
{
	struct file_info *finfo;
	if(fi->fh != 0) {
		if(close(fi->fh) < 0)
			return -errno;
	}

	finfo = get_finfo(path);
	if(finfo) {
		finfo->open_count--;
		pthread_cond_signal(&finfo->cond);
		put_finfo(finfo);
	}
	return 0;
}

static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER;
static int fuse_inited = 0;

static void *tup_fs_init(struct fuse_conn_info *conn)
{
	if(conn) {}
	pthread_mutex_lock(&init_lock);
	fuse_inited = 1;
	pthread_cond_signal(&init_cond);
	pthread_mutex_unlock(&init_lock);
	return NULL;
}

int tup_fs_inited(void)
{
	struct timespec ts;

	ts.tv_sec = time(NULL) + 5;
	ts.tv_nsec = 0;
	pthread_mutex_lock(&init_lock);
	while(!fuse_inited) {
		int rc;
		rc = pthread_cond_timedwait(&init_cond, &init_lock, &ts);
		if(rc != 0) {
			pthread_mutex_unlock(&init_lock);
			if(rc == ETIMEDOUT) {
				fprintf(stderr, "tup error: Timed out waiting for the FUSE file-system to be ready.\n");
				return -1;
			}
			perror("pthread_cond_timedwait");
			return -1;
		}
	}
	pthread_mutex_unlock(&init_lock);
	return 0;
}

struct fuse_operations tup_fs_oper = {
	.getattr = tup_fs_getattr,
	.flush = tup_fs_flush,
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
	.create = tup_fs_create,
	.open = tup_fs_open,
	.read = tup_fs_read,
	.write = tup_fs_write,
	.statfs = tup_fs_statfs,
	.release = tup_fs_release,
	.init = tup_fs_init,
};
