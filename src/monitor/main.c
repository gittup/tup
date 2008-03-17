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
#include "tup/flist.h"
#include "tup/debug.h"
#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/compat.h"

static int watch_path(const char *path, const char *file);
static void handle_event(struct inotify_event *e);
static int handle_delete(const char *path);

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
	lock_fd = open(TUP_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_LOCK);
		return -1;
	}
	if(flock(lock_fd, LOCK_EX) < 0) {
		perror("flock");
		return -1;
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

	if(flock(lock_fd, LOCK_UN) < 0) {
		perror("flock (un)");
		return -1;
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
	static char cname[PATH_MAX];
	struct dircache *dc;
	int lock_mask = IN_MODIFY | IN_ATTRIB | IN_DELETE | IN_MOVE;
	DEBUGP("event: wd=%i, name='%s'\n", e->wd, e->name);

	dc = dircache_lookup(e->wd);
	if(!dc) {
		fprintf(stderr, "Error: dircache entry not found for wd %i\n",
			e->wd);
		return;
	}

	if(e->mask & lock_mask) {
		if(flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
			if(errno == EWOULDBLOCK)
				goto nolock;
			perror("flock");
			return;
		}
	}

	if(canonicalize2(dc->path, e->name, cname, sizeof(cname)) < 0)
		return;
	if(e->mask & IN_CREATE || e->mask & IN_MOVED_TO) {
		if(e->mask & IN_ISDIR) {
			watch_path(dc->path, e->name);
		} else {
			create_name_file(cname);
		}
	}
	if(e->mask & IN_MODIFY || e->mask & IN_ATTRIB) {
		create_tup_file("modify", cname);
	}
	if(e->mask & IN_DELETE || e->mask & IN_MOVED_FROM) {
		handle_delete(cname);
	}
	if(e->mask & lock_mask) {
		flock(lock_fd, LOCK_UN);
	}

nolock:
	if(e->mask & IN_IGNORED) {
		dircache_del(dc);
	}
}

static int handle_delete(const char *path)
{
	tupid_t tupid;

	tupid_from_filename(tupid, path);
	create_tup_file_tupid("delete", tupid);
	delete_tup_file("create", tupid);
	delete_tup_file("modify", tupid);
	return 0;
}
