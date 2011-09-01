#define _POSIX_C_SOURCE 200809L
#define _ATFILE_SOURCE
#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include "tup_fuse_fs.h"
#include "tup/access_event.h"
#include "tup/config.h"
#include "tup/file.h"
#include "tup/fileio.h"
#include "tup/thread_tree.h"
#include "tup/debug.h"
#include "tup/entry.h"
#include "tup/server.h"
#include "tup/db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

static struct thread_root troot = THREAD_ROOT_INITIALIZER;
static int server_mode = 0;
static struct rb_root *server_delete_tree = NULL;

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

void tup_fuse_set_parser_mode(int mode, struct rb_root *delete_tree)
{
	server_mode = mode;
	server_delete_tree = delete_tree;
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
		const char *peeled;

		peeled = peel(path);

		/* TODO: Remove 1 (DOT_DT)? All fuse paths are full */
		if(handle_open_file(ACCESS_WRITE, peeled, finfo, 1) < 0) {
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
		pthread_mutex_lock(&lock);
		myfile = filenum;
		filenum++;
		pthread_mutex_unlock(&lock);

		if(snprintf(map->tmpname, size, TUP_TMP "/%x", myfile) >= size) {
			fprintf(stderr, "tup internal error: mapping tmpname is sized incorrectly.\n");
			return NULL;
		}

		list_add(&map->list, &finfo->mapping_list);
		put_finfo(finfo);
	}
	return map;
}

static struct mapping *find_mapping(struct file_info *finfo, const char *path)
{
	const char *peeled;
	struct mapping *map;

	peeled = peel(path);
	list_for_each_entry(map, &finfo->mapping_list, list) {
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

#ifdef __APPLE__
	/* OSX will fail to return a valid pgid for a zombie process.  However,
	 * for some reason when using 'ar' to create archives, a zombie libtool
	 * process will call 'unlink' on the .fuse_hidden file. If we ignore
	 * that check, then tup will save the .fuse_hidden file as a separate
	 * output because hidden files are ignored.
	 */
	if(pgid == -1 && errno == ESRCH) {
		return 0;
	}
#endif

	if(getpgid(0) != pgid) {
		if(server_debug_enabled()) {
			fprintf(stderr, "[33mtup fuse warning: Process pid=%i, uid=%i, gid=%i is trying to access the tup server's fuse filesystem.[0m\n",
					fuse_get_context()->pid, fuse_get_context()->uid, fuse_get_context()->gid);
		}
		return -1;
	}
	return 0;
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
		}
		put_finfo(finfo);
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
		rc = 0;
		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
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
		put_finfo(finfo);
	}

	/* First we get a getattr("@tup@"), then we get a
	 * getattr("@tup@/CONFIG_FOO"). So first time we return success so fuse
	 * will assume the directory is there, then the second time we keep
	 * track of the variable and return failure because we're not actually
	 * going to open anything.
	 */
	if(strncmp(peeled, TUP_VAR_VIRTUAL_DIR, TUP_VAR_VIRTUAL_DIR_LEN) == 0) {
		const char *var = peeled + TUP_VAR_VIRTUAL_DIR_LEN;
		if(var[0] == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		} else {
			/* skip '/' */
			var++;

			if(finfo) {
				finfo_lock(finfo);
				/* TODO: 1 is always top */
				if(handle_open_file(ACCESS_VAR, var, finfo, 1) < 0) {
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
	if(!S_ISDIR(stbuf->st_mode))
		tup_fuse_handle_file(path, ACCESS_READ);

	return rc;
}

static int tup_fs_access(const char *path, int mask)
{
	int res;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;
	struct tmpdir *tmpdir;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);

	/* OSX will call access() on the virtual directory before calling
	 * getattr() on the variable name, so we check for that here.
	 */
	if(strncmp(peeled, TUP_VAR_VIRTUAL_DIR, TUP_VAR_VIRTUAL_DIR_LEN) == 0) {
		const char *var = peeled + TUP_VAR_VIRTUAL_DIR_LEN;
		if(var[0] == 0) {
			return 0;
		}
	}

	finfo = get_finfo(path);
	if(finfo) {
		int entry_found = 0;
		int rc = 0;

		map = find_mapping(finfo, path);
		if(map)
			peeled = map->tmpname;

		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				/* For a temporary directory, just use the same
				 * access permissions as the top-level directory.
				 * This could be finer grained to use the actual
				 * permissions assigned in mkdir for a temp
				 * directory.
				 */
				if(faccessat(tup_top_fd(), ".", mask, AT_SYMLINK_NOFOLLOW) < 0)
					rc = -errno;
				entry_found = 1;
				break;
			}
		}
		put_finfo(finfo);
		if(entry_found)
			return rc;
	}

	/* This is preceded by a getattr - no need to handle a read event */
	res = faccessat(tup_top_fd(), peeled, mask, AT_SYMLINK_NOFOLLOW);
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

	res = readlinkat(tup_top_fd(), peeled, buf, size - 1);
	if (res == -1)
		return -errno;
	tup_fuse_handle_file(path, ACCESS_READ);

	buf[res] = '\0';
	return 0;
}

struct readdir_parser_params {
	void *buf;
	fuse_fill_dir_t filler;
};

static int readdir_parser_cb(void *arg, struct tup_entry *tent)
{
	struct readdir_parser_params *rpp = arg;
	if(rpp->filler(rpp->buf, tent->name.s, NULL, 0))
		return -EIO;
	return 0;
}

static int readdir_parser(const char *path, void *buf, fuse_fill_dir_t filler)
{
	struct tup_entry *dtent;
	struct readdir_parser_params rpp = {buf, filler};

	if(gimme_tent(path, &dtent) < 0)
		return -EIO;
	if(!dtent)
		return -EIO;
	if(dtent->tnode.tupid != tup_fuse_server_get_curid()) {
		fprintf(stderr, "tup error: Unable to readdir() on directory '%s'. Run-scripts are currently limited to readdir() only the current directory.\n", path);
		return -EPERM;
	}
	if(tup_db_select_node_dir_glob(readdir_parser_cb, &rpp, dtent->tnode.tupid,
				       "*", -1,  server_delete_tree) < 0)
		return -EIO;
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
	int fd;

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
			rc = readdir_parser(peeled, buf, filler);
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

			if(fstatat(tup_top_fd(), map->tmpname, &st, AT_SYMLINK_NOFOLLOW) < 0) {
				perror("lstat");
				fprintf(stderr, "tup error: Unable to stat temporary file '%s'\n", map->tmpname);
				put_finfo(finfo);
				return -1;
			}
			if(filler(buf, realname, &st, 0))
				break;
		}
		put_finfo(finfo);
	}

	if(is_tmpdir)
		return 0;

	tup_fuse_handle_file(path, ACCESS_READ);
	fd = openat(tup_top_fd(), peeled, O_RDONLY);
	if(fd < 0) {
		perror(peeled);
		fprintf(stderr, "tup error: Unable to open directory for reading entries in readdir().\n");
		return -1;
	}
	dp = fdopendir(fd);
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

	if(context_check() < 0)
		return -EPERM;

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

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		int rc = -1;
		tmpdir = malloc(sizeof *tmpdir);
		if(!tmpdir) {
			perror("malloc");
			rc = -ENOMEM;
		}
		tmpdir->dirname = strdup(peel(path));
		if(!tmpdir->dirname) {
			perror("strdup");
			rc = -ENOMEM;
		}
		if(tmpdir && tmpdir->dirname) {
			list_add(&tmpdir->list, &finfo->tmpdir_list);
			rc = 0;
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
			tup_fuse_handle_file(path, ACCESS_UNLINK);
			return 0;
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to unlink files not created during this job.\n");
	return -EPERM;
}

static int tup_fs_rmdir(const char *path)
{
	struct tmpdir *tmpdir;
	const char *peeled;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	finfo = get_finfo(path);
	if(finfo) {
		peeled = peel(path);
		list_for_each_entry(tmpdir, &finfo->tmpdir_list, list) {
			if(strcmp(tmpdir->dirname, peeled) == 0) {
				list_del(&tmpdir->list);
				free(tmpdir->dirname);
				free(tmpdir);
				put_finfo(finfo);
				return 0;
			}
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to rmdir a directory not created during this job.\n");
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
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to chmod() files not created by this job.\n");
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
	fprintf(stderr, "tup error: Unable to chown() files not created by this job.\n");
	return -EPERM;
}

static int tup_fs_truncate(const char *path, off_t size)
{
	struct mapping *map;
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	/* TODO: error check? */
	tup_fuse_handle_file(path, ACCESS_WRITE);
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
			close(fd);
			put_finfo(finfo);
			return rc;
		}
		put_finfo(finfo);
	}
	fprintf(stderr, "tup error: Unable to truncate() files not created by this job.\n");
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
		}
		put_finfo(finfo);
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
	struct file_info *finfo;

	if(context_check() < 0)
		return -EPERM;

	peeled = peel(path);
	openfile = peeled;

	finfo = get_finfo(path);
	if(finfo) {
		map = find_mapping(finfo, path);
		if(map) {
			openfile = map->tmpname;
		}
		put_finfo(finfo);
	}

	if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
		at = ACCESS_WRITE;
	res = openat(tup_top_fd(), openfile, fi->flags);
	if(res < 0) {
		res = -errno;
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

	if(context_check() < 0)
		return -EPERM;

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

	if(context_check() < 0)
		return -EPERM;

	res = pwrite(fi->fh, buf, size, offset);
	if (res == -1)
		res = -errno;

	return res;
}

static int tup_fs_statfs(const char *path, struct statvfs *stbuf)
{
	int fd;
	int rc = 0;
	const char *peeled;
	struct mapping *map;
	struct file_info *finfo;

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

	fd = openat(tup_top_fd(), peeled, O_RDONLY);
	if(fd < 0)
		return -errno;

	if(fstatvfs(fd, stbuf) < 0)
		rc = -errno;
	close(fd);
	return rc;
}

static int tup_fs_release(const char *path, struct fuse_file_info *fi)
{
	if(path) {}
	if(context_check() < 0)
		return -EPERM;
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
