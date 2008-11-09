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

#include "monitor.h"
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
#include <libgen.h> /* TODO: dirname */
#include "dircache.h"
#include "flist.h"
#include "debug.h"
#include "tupid.h"
#include "fileio.h"
#include "compat.h"
#include "db.h"

static int watch_path(const char *path, const char *file);
static void handle_event(struct inotify_event *e);
static int handle_delete(const char *path);

static int inot_fd;
static int lock_fd;
static int lock_wd;

int monitor(int argc, char **argv)
{
	int x;
	int rc = 0;
	int locked = 0;
	struct timeval t1, t2;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("monitor");
		}
	}

	gettimeofday(&t1, NULL);
	lock_fd = open(TUP_OBJECT_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_OBJECT_LOCK);
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

	if(watch_path(".", "") < 0) {
		rc = -1;
		goto close_inot;
	}

	lock_wd = inotify_add_watch(inot_fd, TUP_OBJECT_LOCK, IN_OPEN|IN_CLOSE);
	if(lock_wd < 0) {
		perror("inotify_add_watch");
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

			/* If the lock file is opened, assume we are now
			 * locked out. When the file is closed, check to see
			 * if the lock is available again. We can't just
			 * assume the lock is available when the file is closed
			 * because multiple processes can have the lock shared
			 * at once. Also, we can't count the number of opens
			 * and closes because inotify sometimes slurps some
			 * duplicate events.
			 *
			 * TODO: Reduce duplicate flocking here and in
			 * handle_event()?
			 */
			if(e->wd == lock_wd) {
				if(e->mask & IN_OPEN)
					locked = 1;
				if(e->mask & IN_CLOSE) {
					if(flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
						if(errno != EWOULDBLOCK)
							perror("flock");
					} else {
						locked = 0;
						flock(lock_fd, LOCK_UN);
					}
				}
			} else {
				if(!locked)
					handle_event(e);
			}

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

	/* Skip hidden directories */
	if(file[0] == '.') {
		return 0;
	}

	len = asprintf(&fullpath, "%s%s/", path, file);
	if(len < 0) {
		perror("asprintf");
		return -1;
	}

	/* The canonicalization will remove the trailing '/' temporarily */
	len = canonicalize_string(fullpath, len);

	if(stat(fullpath, &buf) != 0) {
		perror(fullpath);
		rc = -1;
		goto out_free;
	}

	if(S_ISREG(buf.st_mode)) {
		create_name_file(fullpath);
		goto out_free;
	}
	if(S_ISDIR(buf.st_mode)) {
		create_dir_file(fullpath);
	} else {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			fullpath);
		rc = -1;
		goto out_free;
	}

	/* This is totally valid since we have enough space from the asprintf
	 * above, and the canonicalize function can only make then len smaller.
	 */
	fullpath[len] = '/';
	len++;
	fullpath[len] = 0;

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

	/* Skip hidden files */
	if(e->name[0] == '.')
		return;

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

	/* Makefile gets special treatment, since we have to mark the
	 * directory as needing update.
	 */
	if(strcmp(e->name, "Makefile") == 0) {
		if(canonicalize(dc->path, cname, sizeof(cname)) < 0)
			return;
		create_dir_file(cname);
	}

	/* Not a Makefile, so canonicalize the full filename into cname for
	 * the rest of the function.
	 */
	if(canonicalize2(dc->path, e->name, cname, sizeof(cname)) < 0)
		return;
	if(e->mask & IN_CREATE || e->mask & IN_MOVED_TO) {
		if(e->mask & IN_ISDIR) {
			watch_path(dc->path, e->name);
		} else {
			char *slash;
			create_name_file(cname);
			slash = strrchr(cname, '/');
			if(slash) {
				*slash = 0;
				create_dir_file(cname);
				*slash = '/';
			} else {
				create_dir_file(".");
			}
		}
	}
	if(e->mask & IN_MODIFY || e->mask & IN_ATTRIB) {
		new_tupid_t tupid;
		tupid = create_name_file(cname);
		if(tupid < 0)
			return;
		tup_db_exec("update node set flags=%i where id=%lli",
			    TUP_FLAGS_MODIFY, tupid);
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
	{
		/* TODO */
		char *p2;
		char *dir;
		p2 = strdup(path);
		if(!p2) {
			perror("strdup");
			return -1;
		}
		dir = dirname(p2);
		if(create_dir_file(dir) < 0)
			return -1;
		free(p2);
	}
	return 0;
}
