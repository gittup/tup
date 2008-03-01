/*
 * I humbly present the Love-Trowbridge (Lovebridge?) recursive directory
 * scanning algorithm:
 *
 *        Step 1.  Start at initial directory foo.  Add watch.
 *        
 *        Step 2.  Setup handlers for watch created in Step 1.
 *                 Specifically, ensure that a directory created
 *                 in foo will result in a handled CREATE_SUBDIR
 *                 event.
 *        
 *        Step 3.  Read the contents of foo.
 *        
 *        Step 4.  For each subdirectory of foo read in step 3, repeat
 *                 step 1.
 *        
 *        Step 5.  For any CREATE_SUBDIR event on bar, if a watch is
 *                 not yet created on bar, repeat step 1 on bar.
 */

/* _GNU_SOURCE for asprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>
#include "dircache.h"
#include "flist.h"
#include "debug.h"
#include "tupid.h"
#include "mkdirhier.h"
#include "fileio.h"
#include "tup-compat.h"

static int make_tup_filesystem(void);
static int watch_path(const char *path, const char *file);
static void handle_event(struct inotify_event *e);
static int create_name_file(const char *file);
static int create_name_file2(const char *path, const char *file);
static int create_tup_file(const char *path, const char *file, const char *tup);

static int inot_fd;
static int lock_fd;

int main(int argc, char **argv)
{
	int x;
	int rc = 0;
	struct timeval t1, t2;
	const char *path = NULL;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("monitor");
		} else {
			path = argv[x];
		}
	}
	if(!path) {
		fprintf(stderr, "Usage: %s [-d] path_to_watch\n", argv[0]);
		return 1;
	}

	gettimeofday(&t1, NULL);
	if(make_tup_filesystem() < 0) {
		fprintf(stderr, "Unable to create tup filesystem hierarchy.\n");
		return 1;
	}

	inot_fd = inotify_init();
	if(inot_fd < 0) {
		perror("inotify_init");
		return -1;
	}

	if(watch_path(path, "") < 0) {
		rc = -1;
		goto close_inot;
	}

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	while((x = read(inot_fd, buf, sizeof(buf))) > 0) {
		int offset = 0;

		while(offset < x) {
			struct inotify_event *e = (void*)((char*)buf + offset);

			handle_event(e);
			offset += sizeof(*e) + e->len;
		}
	}

close_inot:
	close(inot_fd);
	close(lock_fd);
	return rc;
}

static int make_tup_filesystem(void)
{
	unsigned int x;
	char pathnames[][13] = {
		".tup/create/",
		".tup/modify/",
		".tup/delete/",
		".tup/object/",
	};
	for(x=0; x<sizeof(pathnames) / sizeof(pathnames[0]); x++) {
		printf("PATH: '%s'\n", pathnames[x]);
		if(mkdirhier(pathnames[x]) < 0) {
			return -1;
		}
	}

	lock_fd = open(TUP_LOCK, O_RDONLY | O_CREAT, 0666);
	if(lock_fd < 0) {
		perror(TUP_LOCK);
		return -1;
	}
	return 0;
}

static int watch_path(const char *path, const char *file)
{
	int wd;
	int rc = 0;
	int len;
	uint32_t mask;
	struct flist f;
	struct stat buf;
	char *fullpath;

	/* Skip our own directory */
	if(strcmp(file, ".tup") == 0) {
		return 0;
	}

	len = asprintf(&fullpath, "%s%s/", path, file);
	if(len < 0) {
		perror("asprintf");
		return -1;
	}

	/* Remove trailing / temporarily-ish */
	fullpath[len-1] = 0;
	if(stat(fullpath, &buf) != 0) {
		perror(fullpath);
		rc = -1;
		goto out_free;
	}
	if(S_ISREG(buf.st_mode)) {
		create_name_file(fullpath);
		goto out_free;
	}
	if(!S_ISDIR(buf.st_mode)) {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			fullpath);
		rc = -1;
		goto out_free;
	}
	fullpath[len-1] = '/';

	DEBUGP("add watch: '%s'\n", fullpath);

	mask = IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVE;
	wd = inotify_add_watch(inot_fd, fullpath, mask);
	if(wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto out_free;
	}
	dircache_add(wd, fullpath);
	flist_foreach(&f, fullpath) {
		if(strcmp(f.filename, ".") == 0 ||
		   strcmp(f.filename, "..") == 0)
			continue;
		watch_path(fullpath, f.filename);
	}
	/* dircache assumes ownership of fullpath memory */
	return 0;

out_free:
	free(fullpath);
	return rc;
}

static void handle_event(struct inotify_event *e)
{
	DEBUGP("event: wd=%i, name='%s'\n", e->wd, e->name);

	/* TODO: Handle MOVED_FROM/MOVED_TO, DELETE events */
	if(e->len > 0) {
		printf("%08x:%s%s\n", e->mask,
		       dircache_lookup(e->wd), e->name);
	} else {
		printf("%08x:%s\n", e->mask,
		       dircache_lookup(e->wd));
	}

	if(e->mask & IN_CREATE) {
		if(e->mask & IN_ISDIR) {
			watch_path(dircache_lookup(e->wd), e->name);
		} else {
			create_name_file2(dircache_lookup(e->wd), e->name);
		}
	}
	if(e->mask & IN_MODIFY || e->mask & IN_ATTRIB) {
		create_tup_file(dircache_lookup(e->wd), e->name, "modify");
	}
	if(e->mask & IN_DELETE) {
		create_tup_file(dircache_lookup(e->wd), e->name, "delete");
	}
	if(e->mask & IN_IGNORED) {
		dircache_del(e->wd);
	}
}

static int create_name_file(const char *file)
{
	return create_name_file2(file, "");
}

static int create_name_file2(const char *path, const char *file)
{
	int fd;
	int rc = -1;
	int len;
	char tupfilename[] = ".tup/object/" SHA1_X "/.name";
	static char read_filename[PATH_MAX];

	path = tupid_from_path_filename(tupfilename + 12, path, file);

	DEBUGP("create tup file '%s' containing '%s%s'.\n",
	       tupfilename, path, file);
	if(mkdirhier(tupfilename) < 0)
		return -1;
        fd = open(tupfilename, O_RDONLY);
        if(fd < 0) {
                fd = open(tupfilename, O_WRONLY | O_CREAT, 0666);
                if(fd < 0) {
                        perror("open");
                        return -1;
                }
		if(write_all(fd, path, strlen(path), tupfilename) < 0)
			goto err_out;
		if(write_all(fd, file, strlen(file), tupfilename) < 0)
			goto err_out;
		if(write_all(fd, "\n", 1, tupfilename) < 0)
			goto err_out;
		create_tup_file(path, file, "create");
        } else {
		int pathlen = strlen(path);

                len = read(fd, read_filename, sizeof(read_filename) - 1);
                if(len < 0) {
                        perror("read");
			goto err_out;
                }
                read_filename[len] = 0;

		if(memcmp(read_filename, path, pathlen) != 0 ||
		   memcmp(read_filename+pathlen, file, strlen(file)) != 0) {
                        fprintf(stderr, "Gak! SHA1 collision? Requested "
                                "file '%s' doesn't match stored file '%s' for "
                                "in '%s'\n", file, read_filename, tupfilename);
			goto err_out;
                }
        }
	rc = 0;
err_out:
        close(fd);
        return rc;
}

static int create_tup_file(const char *path, const char *file, const char *tup)
{
	int rc;
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	if(flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
		/* tup must be running a wrapped command */
		if(errno == EWOULDBLOCK)
			return 0;
		/* or some other error occurred */
		perror("flock");
		return -1;
	}
	memcpy(filename + 5, tup, 6);
	path = tupid_from_path_filename(filename + 12, path, file);

	DEBUGP("create %s file: %s\n", tup, filename);
	rc = create_if_not_exist(filename);
	flock(lock_fd, LOCK_UN);
	return rc;
}
