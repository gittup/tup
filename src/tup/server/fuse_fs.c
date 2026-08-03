/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2026  Mike Shal <marfey@gmail.com>
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
#include "tup/ccache.h"
#include "tup/debug.h"
#include "tup/server.h"
#include "tup/container.h"
#include "tup/entry.h"
#include "tup/pel_group.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/resource.h>

#ifdef FUSE_NFS_WORKAROUND
#include <dlfcn.h>
#endif

static struct thread_root troot = THREAD_ROOT_INITIALIZER;
static int server_mode = 0;
static pid_t ourpgid;
static int max_open_files = 128;

#ifdef FUSE_NFS_WORKAROUND
static int readdir_getattr_workaround = 0;

/* Some FUSE implementations (e.g. Fuse-T) use NFS internally, where the
 * NFS client will stat every directory entry of each readdir.
 *
 * The exact conditions of this behavior are not known, and we therefore
 * probe for it at startup. By listing a virtual directory, we watch for
 * whether a stat is issued for a sentinel file from that listing. If
 * this stat occurs, the workaround is enabled. The main thread then
 * stats a separate "done" file to finalize the probe.
 *
 * UNCHECKED -> readdir on probe dir           -> SENTINEL
 * SENTINEL  -> getattr on sentinel            -> enable workaround
 *              getattr on done file           -> DONE
 */
enum nfs_probe_state {
	NFS_PROBE_UNCHECKED,
	NFS_PROBE_SENTINEL,
	NFS_PROBE_DONE
};

static volatile enum nfs_probe_state nfs_probe = NFS_PROBE_UNCHECKED;

#define NFS_PROBE_DIR_NAME "@nfs_probe@"
#define NFS_PROBE_SENTINEL_NAME ".nfs_sentinel"
#define NFS_PROBE_DONE_NAME ".nfs_done"
#endif

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
			rlim_t half = rlim.rlim_cur / 2;
			if(half > INT_MAX)
				half = INT_MAX;
			max_open_files = (int)half;
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
	if(strstr(path, "/.svn") != NULL)
		return 1;
	if(strstr(path, "/.bzr") != NULL)
		return 1;
	if(is_ccache_path(path))
		return 1;
	if(is_appledouble(path))
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
		const char *slash;

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

static struct mapping *add_mapping_internal(struct file_info *finfo, const char *path)
{
	static int filenum = 0;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	struct mapping *map = NULL;
	int size;
	int myfile;
	const char *peeled;

	peeled = peel(path);

	if(!is_hidden(peeled)) {
		if(handle_open_file(ACCESS_WRITE, peeled, finfo) < 0) {
			/* TODO: Set failure on internal server? */
			fprintf(stderr, "tup internal error: handle open file failed\n");
			return NULL;
		}
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

	TAILQ_INSERT_TAIL(&finfo->mapping_list, map, list);
	return map;
}

static struct mapping *add_mapping(const char *path)
{
	struct file_info *finfo;
	struct mapping *map = NULL;

	finfo = get_finfo(path);
	if(finfo) {
		map = add_mapping_internal(finfo, path);
		put_finfo(finfo);
	}
	return map;
}

static struct mapping *find_mapping(struct file_info *finfo, const char *path)
{
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);
	TAILQ_FOREACH(map, &finfo->mapping_list, list) {
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
	if(strncmp(path, "/dev", 4) == 0)
		return 1;
	if(strncmp(path, "/sys", 4) == 0)
		return 1;
	if(strncmp(path, "/proc", 5) == 0)
		return 1;
	if(is_ccache_path(path))
		return 1;
	if(is_appledouble(path))
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

static const char *get_virtual_var(const char *peeled)
{
	const char *stripped;

	if(strncmp(peeled, get_tup_top(), get_tup_top_len()) == 0) {
		peeled += get_tup_top_len();
		if(peeled[0] != '/')
			return NULL;
		peeled++;
		stripped = peeled;
	} else {
		return NULL;
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
#ifdef FUSE3
static int tup_fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
#else
static int tup_fs_getattr(const char *path, struct stat *stbuf)
#endif
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct tmpdir *tmpdir;
	struct file_info *finfo;
	const char *var;
	const char *stripped = NULL;
	int rc;
	int skip_read = 0;
#ifdef FUSE_NFS_WORKAROUND
	int from_sticky = 0;
#endif

#ifdef FUSE3
	(void) fi;
#endif
	if(context_check() < 0)
		return -EPERM;

#ifdef FUSE_NFS_WORKAROUND
	if(nfs_probe != NFS_PROBE_DONE) {
		const char *base = strrchr(path, '/');
		if(base) base++; else base = path;
		if(strcmp(base, NFS_PROBE_DIR_NAME) == 0) {
			memset(stbuf, 0, sizeof(*stbuf));
			stbuf->st_mode = S_IFDIR | 0555;
			stbuf->st_nlink = 2;
			return 0;
		}
		if(nfs_probe == NFS_PROBE_SENTINEL) {
			if(strcmp(base, NFS_PROBE_SENTINEL_NAME) == 0) {
				readdir_getattr_workaround = 1;
				memset(stbuf, 0, sizeof(*stbuf));
				stbuf->st_mode = S_IFREG | 0444;
				return 0;
			}
			if(strcmp(base, NFS_PROBE_DONE_NAME) == 0) {
				nfs_probe = NFS_PROBE_DONE;
				return -ENOENT;
			}
		}
	}
#endif

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
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
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
#ifdef FUSE_NFS_WORKAROUND
		/* Under Fuse-T NFS, readdir is followed by automatic stat of
		 * each directory entry. If this path was recorded in
		 * readdir_sticky, consume it and skip ACCESS_READ later.
		 */
		if(readdir_getattr_workaround) {
			struct string_tree *st;
			st = string_tree_search(&finfo->readdir_sticky, path, strlen(path));
			if(st) {
				string_tree_remove(&finfo->readdir_sticky, st);
				skip_read = 1;
				from_sticky = 1;
			}
		}
#endif
		put_finfo(finfo);
	}

	/* First we get a getattr("@tup@"), then we get a
	 * getattr("@tup@/CONFIG_FOO"). So first time we return success so fuse
	 * will assume the directory is there, then the second time we keep
	 * track of the variable and return failure because we're not actually
	 * going to open anything.
	 */
	var = get_virtual_var(peeled);
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
	if (res == -1) {
		rc = -errno;
	} else {
		rc = 0;
	}

#ifdef FUSE_NFS_WORKAROUND
	/* If this getattr was triggered by the NFS auto-stat that follows
	 * a parser-mode readdir, and the underlying file does not exist on
	 * disk (it's a virtual entry from parser_directory, e.g. a not-yet-
	 * built *.o output), synthesize a successful stat so the kernel's
	 * NFS layer keeps the entry in its readdir result. Otherwise the
	 * shell running the run-script filters virtual entries out of glob
	 * expansion and they never reach the script's output. We also skip
	 * read recording for virtual entries — they're synthetic, not user
	 * intent. Real on-disk entries fall through and record normally.
	 */
	if(from_sticky && rc == -ENOENT) {
		if(fstat(tup_top_fd(), stbuf) == 0) {
			stbuf->st_mode = (stbuf->st_mode & ~S_IFMT) | S_IFREG;
			stbuf->st_size = 0;
			stbuf->st_nlink = 1;
			rc = 0;
		}
	}
#endif

	if(!skip_read)
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

		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				/* For a temporary directory, just use the same
				 * access permissions as the top-level directory.
				 * This could be finer grained to use the actual
				 * permissions assigned in mkdir for a temp
				 * directory.
				 */
				if(faccessat(tup_top_fd(), ".", mask, 0) < 0)
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
	var = get_virtual_var(peeled);
	if(var && var[0] == 0) {
		return 0;
	}

	/* This is preceded by a getattr - no need to handle a read event */
	res = faccessat(tup_top_fd(), peeled, mask, 0);
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
	const char *stripped = NULL;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	finfo = get_finfo(path);
	if(finfo) {
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
			if(mfiller(buf, de->d_name, &st, 0))
				break;
	}
}

static int fill_actual_directory(const char *peeled, void *buf,
				 fuse_fill_dir_t filler, int ignore_dot_tup)
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
	return 0;
}

#ifdef FUSE_NFS_WORKAROUND
/* Parser-mode readdir is served from parser_directory, not the real fs,
 * so the readdir_sticky population path in tup_fs_readdir (which walks
 * the on-disk directory) never sees these entries. Snapshot the names
 * as the parser fills them and feed them into readdir_sticky in the
 * caller, so the kernel's post-readdir auto-getattr on each virtual
 * entry is recognized as a stat-only access and not recorded as a read
 * of a generated file.
 */
struct readdir_sticky_collect {
	char **names;
	int count;
	int cap;
};

static void collect_readdir_sticky(const char *name, void *vctx)
{
	struct readdir_sticky_collect *c = vctx;
	char *dup;

	if(c->count == c->cap) {
		int newcap = c->cap ? c->cap * 2 : 16;
		char **newnames = realloc(c->names, newcap * sizeof(*newnames));
		if(!newnames)
			return;
		c->names = newnames;
		c->cap = newcap;
	}
	dup = strdup(name);
	if(!dup)
		return;
	c->names[c->count++] = dup;
}
#endif

static int readdir_parser(const char *fuse_path, const char *path, void *buf,
			  fuse_fill_dir_t filler, struct file_info *finfo)
{
	if(strncmp(path, get_tup_top(), get_tup_top_len()) == 0) {
		void (*on_entry)(const char *, void *) = NULL;
		void *ctx_ptr = NULL;
		int rc;
#ifdef FUSE_NFS_WORKAROUND
		struct readdir_sticky_collect collect = { NULL, 0, 0 };
		if(readdir_getattr_workaround && finfo) {
			on_entry = collect_readdir_sticky;
			ctx_ptr = &collect;
		}
#else
		(void) fuse_path;
		(void) finfo;
#endif
		rc = tup_fuse_server_get_dir_entries(
			path + get_tup_top_len(), buf, filler,
			on_entry, ctx_ptr);
#ifdef FUSE_NFS_WORKAROUND
		if(on_entry) {
			int fuse_pathlen = strlen(fuse_path);
			int i;
			if(rc == 0 && collect.count > 0) {
				for(i = 0; i < collect.count; i++) {
					const char *name = collect.names[i];
					struct string_tree *st;
					char *fullpath;
					int namelen = strlen(name);
					int fulllen = fuse_pathlen + 1 + namelen;

					fullpath = malloc(fulllen + 1);
					if(!fullpath)
						continue;
					memcpy(fullpath, fuse_path, fuse_pathlen);
					fullpath[fuse_pathlen] = '/';
					memcpy(fullpath + fuse_pathlen + 1,
					       name, namelen + 1);

					st = malloc(sizeof(*st));
					if(!st) {
						free(fullpath);
						continue;
					}
					st->s = fullpath;
					st->len = fulllen;
					if(string_tree_insert(
						&finfo->readdir_sticky, st) < 0) {
						free(fullpath);
						free(st);
					}
				}
			}
			for(i = 0; i < collect.count; i++)
				free(collect.names[i]);
			free(collect.names);
		}
#endif
		if(rc < 0)
			return -EPERM;
	} else {
		/* t4052 */
		return fill_actual_directory(path, buf, filler, 0);
	}
	return 0;
}

#ifdef FUSE3
static int tup_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
#else
static int tup_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
#endif
{
	const char *peeled;
	struct file_info *finfo;
	int is_tmpdir = 0;

	(void) offset;
	(void) fi;
#ifdef FUSE3
	(void) flags;
#endif

	if(context_check() < 0)
		return -EPERM;

#ifdef FUSE_NFS_WORKAROUND
	if(nfs_probe == NFS_PROBE_UNCHECKED) {
		const char *base = strrchr(path, '/');
		if(base) base++; else base = path;
		if(strcmp(base, NFS_PROBE_DIR_NAME) == 0) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_mode = S_IFREG | 0444;
			filler(buf, NFS_PROBE_SENTINEL_NAME, &st, 0);
			nfs_probe = NFS_PROBE_SENTINEL;
			return 0;
		}
	}
#endif

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
#ifdef FUSE_NFS_WORKAROUND
			/* If this readdir is the kernel's post-exec bundle-root
			 * probe (predicted by tup_fs_read on a shebang script)
			 * AND the path is NOT a directory the parser knows
			 * about, fulfill it silently from the real filesystem
			 * so the exec syscall proceeds. parser_directory does
			 * not contain these bundle-root paths and would
			 * normally fail with -EPERM, which would set
			 * server_fail and trip an access-violation error.
			 *
			 * We only bypass when the path is OUTSIDE
			 * parser_directory because passing real-fs contents
			 * to a path the parser owns would pollute the NFS
			 * readdir cache with stale entries (missing virtual
			 * outputs) for the script's own later readdir of the
			 * same path. See t8079-run-variant where the script
			 * is at the parser dir's root.
			 */
			struct string_tree *bundle_st;
			bundle_st = string_tree_search(
				&finfo->open_readdir_sticky,
				path, strlen(path));
			if(bundle_st) {
				const char *rel = peeled + get_tup_top_len();
				int peeled_in_tup_top = strncmp(peeled, get_tup_top(), get_tup_top_len()) == 0;
				int in_parser_dir = peeled_in_tup_top &&
					tup_fuse_server_has_dir(rel);
				/* string_tree_remove() frees bundle_st->s
				 * internally; we own the wrapper struct.
				 */
				string_tree_remove(
					&finfo->open_readdir_sticky,
					bundle_st);
				free(bundle_st);
				if(!in_parser_dir) {
					rc = fill_actual_directory(peeled, buf, filler, 0);
					put_finfo(finfo);
					return rc;
				}
			}
#endif
			rc = readdir_parser(path, peeled, buf, filler, finfo);
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
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				is_tmpdir = 1;
				break;
			}
		}

		/* Check any mappings to see if there are extra files that
		 * we need to add to the list in addition to whatever we
		 * get from the real opendir/readdir (if applicable).
		 */
		TAILQ_FOREACH(map, &finfo->mapping_list, list) {
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
			if(mfiller(buf, realname, &st, 0))
				break;
		}

		/* Check any tmpdir subdirs, and add them to the list */
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
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

				if(mfiller(buf, realname, &st, 0))
					break;
			}
		}

		put_finfo(finfo);
	}

	if(is_tmpdir)
		return 0;

	tup_fuse_handle_file(path, NULL, ACCESS_READ);

	/* If finfo is NULL, we're outside of tup, so we don't need to ignore
	 * any files called '.tup' in that case.
	 */
	int rc = fill_actual_directory(peeled, buf, filler, finfo != NULL);

#ifdef FUSE_NFS_WORKAROUND
	/* Under Fuse-T NFS, readdir causes an immediate stat to each
	 * directory entry, so add them to a tree that tells getattr to
	 * avoid dispatching one ACCESS_READ for each.
	 */
	if(finfo != NULL && rc >= 0 && readdir_getattr_workaround) {
		finfo_lock(finfo);
		int fd;
		fd = openat(tup_top_fd(), peeled, O_RDONLY);
		if(fd >= 0) {
			DIR *dp;
			dp = fdopendir(fd);
			if(dp) {
				struct dirent *de;
				int pathlen = strlen(path);

				while((de = readdir(dp)) != NULL) {
					struct string_tree *st;
					char *fullpath;
					int namelen = strlen(de->d_name);
					int fulllen = pathlen + 1 + namelen;

					fullpath = malloc(fulllen + 1);
					if(!fullpath)
						break;
					memcpy(fullpath, path, pathlen);
					fullpath[pathlen] = '/';
					memcpy(fullpath + pathlen + 1, de->d_name, namelen + 1);

					st = malloc(sizeof(*st));
					if(!st) {
						free(fullpath);
						break;
					}
					st->s = fullpath;
					st->len = fulllen;
					if(string_tree_insert(&finfo->readdir_sticky, st) < 0) {
						/* Duplicate entry, already in set */
						free(fullpath);
						free(st);
					}
				}
				closedir(dp);
			} else {
				close(fd);
			}
		}
		finfo_unlock(finfo);
	}
#endif

	return rc;
}

static int mknod_internal(const char *path, mode_t mode, int flags, int close_fd)
{
	int rc;
	struct mapping *map;

	if(context_check() < 0)
		return -EPERM;

	/* t5119 - Make sure we could write to the actual underlying directory,
	 * since that can affect program behavior. For example, inkscape uses
	 * fontconfig, which tries to write .uuid files in /usr/. If we report
	 * that the write succeeds, we'll have issues with unspecified outputs.
	 */
	char *dir;
	char *dirtmp = strdup(path);
	if(!dirtmp) {
		return -ENOMEM;
	}
	dir = dirname(dirtmp);

	/* Use tup_fs_access in case it's a directory we mkdir'd during program
	 * execution.
	 */
	rc = tup_fs_access(dir, W_OK);
	free(dirtmp);
	if(rc < 0) {
		return -errno;
	}

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
	const char *peeled;

	if(mode) {}

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
	if(ignore_file(peeled)) {
		int rc;

		/* Things like ccache need to just call mkdir rather than use
		 * our temporary directories.
		 */
		rc = mkdir(peeled, mode);
		if(rc < 0)
			return -errno;
		return 0;
	}

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
				TAILQ_INSERT_TAIL(&finfo->tmpdir_list, tmpdir, list);
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
			del_map(&finfo->mapping_list, map);
			put_finfo(finfo);
			tup_fuse_handle_file(path, NULL, ACCESS_UNLINK);
			return 0;
		}
		put_finfo(finfo);
	}
	if(strstr(path, ".fuse_hidden") != NULL) {
		/* Similar to the rename check for .fuse_hidden, this shows up
		 * in Arch sometimes.
		 */
		const char *peeled = peel(path);
		int res;

		res = unlink(peeled);
		if(res < 0)
			return -errno;
		return 0;
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
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strncmp(tmpdir->dirname, peeled, len) == 0 && tmpdir->dirname[len] == '/') {
				put_finfo(finfo);
				return -ENOTEMPTY;
			}
		}
		// Ensure that there are no files in the directory
		TAILQ_FOREACH(map, &finfo->mapping_list, list) {
			if (strncmp(map->realname, peeled, len) == 0 && map->realname[len] == '/') {
				put_finfo(finfo);
				return -ENOTEMPTY;
			}
		}

		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				TAILQ_REMOVE(&finfo->tmpdir_list, tmpdir, list);
				free(tmpdir->dirname);
				free(tmpdir);
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to rmdir a directory not created during this job: %s\n", peel(path));
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

#ifdef FUSE3
static int tup_fs_rename(const char *from, const char *to, unsigned int flags)
#else
static int tup_fs_rename(const char *from, const char *to)
#endif
{
	struct file_info *finfo;
	const char *peelfrom;
	const char *peelto;
	struct mapping *map;

#ifdef FUSE3
	(void) flags;
#endif
	if(context_check() < 0)
		return -EPERM;

	peelfrom = peel(from);
	peelto = peel(to);

	finfo = get_finfo(to);
	if(finfo) {
		struct tmpdir *tmpdir;
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peelfrom) == 0) {
				free(tmpdir->dirname);
				tmpdir->dirname = strdup(peelto);
				if(!tmpdir->dirname) {
					perror("strdup");
					put_finfo(finfo);
					return -ENOMEM;
				}
				put_finfo(finfo);
				return 0;
			}
		}

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
			del_map(&finfo->mapping_list, map);
		}

		map = find_mapping(finfo, from);
		if(!map) {
			put_finfo(finfo);
			return -ENOENT;
		}

		if(strstr(peelto, ".fuse_hidden") != NULL) {
			/* If we're renaming to a .fuse_hidden file, treat it
			 * as if the source was unlinked. This happens
			 * sometimes in an Arch VM where a deleted file shows
			 * up in FUSE as a rename to a .fuse_hidden file,
			 * especially in t4017 for some reason.
			 */
			unlinkat(tup_top_fd(), map->tmpname, 0);
			del_map(&finfo->mapping_list, map);
			put_finfo(finfo);
			tup_fuse_handle_file(from, NULL, ACCESS_UNLINK);
		} else {
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
	}

	return 0;
}

static int tup_fs_link(const char *from, const char *to)
{
	if(from || to) {}

	fprintf(stderr, "tup error: hard links are not supported.\n");
	return -EPERM;
}

#ifdef FUSE3
static int tup_fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
#else
static int tup_fs_chmod(const char *path, mode_t mode)
#endif
{
	struct mapping *map;
	struct file_info *finfo;
	const char *peeled;
	struct tmpdir *tmpdir;

#ifdef FUSE3
	(void) fi;
#endif
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
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to chmod() files not created by this job: %s\n", peel(path));
	return -EPERM;
}

#ifdef FUSE3
static int tup_fs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
#else
static int tup_fs_chown(const char *path, uid_t uid, gid_t gid)
#endif
{
	struct mapping *map;
	struct file_info *finfo;
	const char *peeled;
	struct tmpdir *tmpdir;

#ifdef FUSE3
	(void) fi;
#endif
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
		peeled = peel(path);
		TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to chown() files not created by this job: %s\n", peel(path));
	return -EPERM;
}

#ifdef FUSE3
static int tup_fs_truncate(const char *path, off_t size, struct fuse_file_info *fi)
#else
static int tup_fs_truncate(const char *path, off_t size)
#endif
{
	struct mapping *map;
	struct file_info *finfo;
	const char *peeled;
	struct tup_entry *match = NULL;

#ifdef FUSE3
	(void) fi;
#endif
	if(context_check() < 0)
		return -EPERM;

	/* TODO: error check? */
	tup_fuse_handle_file(path, NULL, ACCESS_WRITE);
	peeled = peel(path);
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
		} else {
			if(exclusion_match(stderr, &finfo->exclusion_root, peeled, &match) < 0) {
				put_finfo(finfo);
				return -ENOSYS;
			}
		}
		put_finfo(finfo);
	}
	if(match || is_hidden(peeled)) {
		if(truncate(peeled, size) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to truncate() files not created by this job: %s\n", peel(path));
	return -EPERM;
}

#ifdef FUSE3
static int tup_fs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi)
#else
static int tup_fs_utimens(const char *path, const struct timespec ts[2])
#endif
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;
	struct tup_entry *match = NULL;

#ifdef FUSE3
	(void) fi;
#endif
	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
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
		} else {
			struct tmpdir *tmpdir;
			if(exclusion_match(stderr, &finfo->exclusion_root, peeled, &match) < 0) {
				put_finfo(finfo);
				return -ENOSYS;
			}
			/* Ignore a touch on a temporary directory */
			TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
				if(strcmp(tmpdir->dirname, peeled) == 0) {
					put_finfo(finfo);
					return 0;
				}
			}
		}
		put_finfo(finfo);
	}
	if(match || is_hidden(peeled)) {
		if(utimensat(tup_top_fd(), peeled, ts, AT_SYMLINK_NOFOLLOW) < 0)
			return -errno;
		return 0;
	}
	fprintf(stderr, "tup error: Unable to utimens() files not created by this job: %s\n", peeled);
	return -EPERM;
}

static int tup_fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int rc;
	struct file_info *finfo;
	rc = mknod_internal(path, mode, fi->flags, 0);
	if(rc < 0)
		return rc;
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
	const char *stripped = NULL;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
	openfile = peeled;

	finfo = get_finfo(path);
	if(finfo) {
		if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
			at = ACCESS_WRITE;
		map = find_mapping(finfo, path);
		if(map) {
			openfile = map->tmpname;
		} else {
#ifdef FUSE3
			struct tup_entry *match = NULL;
			if(exclusion_match(stderr, &finfo->exclusion_root, path, &match) < 0) {
				put_finfo(finfo);
				return -ENOSYS;
			}
			if(at == ACCESS_WRITE && !is_hidden(path) && !match) {
				map = add_mapping_internal(finfo, path);
				openfile = map->tmpname;
			}
#endif
		}

		fd = openat(tup_top_fd(), openfile, fi->flags);
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

#ifdef FUSE_NFS_WORKAROUND
/* Compute the directory the kernel will readdir() after exec'ing a
 * shebang script at `path`. The rule is purely structural (no Launch
 * Services consultation):
 *
 *   cand = parent(path)
 *   while parent(cand) exists:
 *     - if cand.basename == "MacOS" and parent.basename == "Contents":
 *         cand = parent.parent (jump past the Contents/MacOS chain)
 *     - elif cand.basename has a dot:
 *         - if parent.basename == "MacOS" and parent.parent.basename ==
 *           "Contents":  cand = parent.parent.parent
 *         - elif parent.basename has a dot:  cand = parent
 *         - else: stop
 *     - else: stop
 *
 * See repro/SUMMARY.md for the full forensic evidence (29 scenarios).
 * `out` is filled with the predicted directory's FUSE-mount-relative
 * path (same shape as `path` — e.g. /@tupjob-N/abs/...).
 */
static int has_dot(const char *seg, int seglen)
{
	int i;
	for(i = 0; i < seglen; i++)
		if(seg[i] == '.')
			return 1;
	return 0;
}

static void last_segment(const char *path, int pathlen,
			 const char **seg_out, int *seglen_out)
{
	int i = pathlen;
	while(i > 0 && path[i-1] == '/')
		i--;
	int end = i;
	while(i > 0 && path[i-1] != '/')
		i--;
	*seg_out = path + i;
	*seglen_out = end - i;
}

static int parent_path(const char *path, int pathlen)
{
	int i = pathlen;
	while(i > 0 && path[i-1] == '/')
		i--;
	while(i > 0 && path[i-1] != '/')
		i--;
	while(i > 1 && path[i-1] == '/')
		i--;
	return i;
}

static int compute_bundle_root(const char *path, char *out, size_t out_size)
{
	int cand_len, parent_len, grand_len;
	const char *seg;
	int seglen;
	const char *psg;
	int pseglen;

	cand_len = parent_path(path, strlen(path));
	if(cand_len <= 0)
		return -1;

	while(1) {
		parent_len = parent_path(path, cand_len);
		if(parent_len <= 0)
			break;

		last_segment(path, cand_len, &seg, &seglen);
		last_segment(path, parent_len, &psg, &pseglen);

		if(seglen == 5 && memcmp(seg, "MacOS", 5) == 0 &&
		   pseglen == 8 && memcmp(psg, "Contents", 8) == 0) {
			grand_len = parent_path(path, parent_len);
			if(grand_len <= 0)
				break;
			cand_len = grand_len;
			continue;
		}

		if(has_dot(seg, seglen)) {
			if(pseglen == 5 && memcmp(psg, "MacOS", 5) == 0) {
				int gp_len = parent_path(path, parent_len);
				const char *gpsg;
				int gpseglen;
				last_segment(path, gp_len, &gpsg, &gpseglen);
				if(gpseglen == 8 && memcmp(gpsg, "Contents", 8) == 0) {
					int ggp_len = parent_path(path, gp_len);
					if(ggp_len <= 0)
						break;
					cand_len = ggp_len;
					continue;
				}
			}
			if(has_dot(psg, pseglen)) {
				cand_len = parent_len;
				continue;
			}
		}

		break;
	}

	if((size_t)cand_len >= out_size)
		return -1;
	memcpy(out, path, cand_len);
	out[cand_len] = '\0';
	return 0;
}

static void maybe_record_shebang(const char *path, const char *buf,
				 int res, struct file_info *finfo)
{
	char bundle_root[PATH_MAX];
	struct string_tree *st;
	int len;

	if(server_mode != SERVER_PARSER_MODE)
		return;
	if(res < 2)
		return;
	if(buf[0] != '#' || buf[1] != '!')
		return;
	if(compute_bundle_root(path, bundle_root, sizeof(bundle_root)) < 0)
		return;

	len = strlen(bundle_root);
	st = malloc(sizeof(*st));
	if(!st)
		return;
	st->s = strdup(bundle_root);
	if(!st->s) {
		free(st);
		return;
	}
	st->len = len;
	/* finfo_lock is already held by get_finfo() in our caller. */
	if(string_tree_insert(&finfo->open_readdir_sticky, st) < 0) {
		/* duplicate, OK */
		free(st->s);
		free(st);
	}
}
#endif

static int tup_fs_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	int res;
	int fd;
#ifdef FUSE_NFS_WORKAROUND
	struct file_info *predict_finfo = NULL;
#endif

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

#ifdef FUSE_NFS_WORKAROUND
	if(offset == 0 && res >= 2 && server_mode == SERVER_PARSER_MODE) {
		predict_finfo = get_finfo(path);
		if(predict_finfo) {
			maybe_record_shebang(path, buf, res, predict_finfo);
			put_finfo(predict_finfo);
		}
	}
#endif

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
			TAILQ_FOREACH(tmpdir, &finfo->tmpdir_list, list) {
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

#ifdef FUSE3
static void *tup_fs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
#else
static void *tup_fs_init(struct fuse_conn_info *conn)
#endif
{
#ifdef FUSE3
	(void) cfg;

	/* Tup doesn't support readdirplus. tup_fs_readdir() could be changed
	 * to be similar to fuse/example/passthrough_fh.c (along with
	 * simple opendir() / releasedir() wrappers), but I'm not sure how to
	 * support the mapped files & tmpdirs correctly.
	 */
	conn->want = conn->want & ~FUSE_CAP_READDIRPLUS;
#else
	(void) conn;
#endif
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
