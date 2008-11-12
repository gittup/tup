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
#include <sys/inotify.h>
#include <sys/time.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "dircache.h"
#include "flist.h"
#include "debug.h"
#include "fileio.h"
#include "config.h"
#include "db.h"
#include "lock.h"

#define MONITOR_PID_CFG "monitor pid"

static int watch_path(const char *path, const char *file);
static void handle_event(struct inotify_event *e);
static void sighandler(int sig);

static int inot_fd;
static int obj_wd;
static int mon_wd;
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};

int monitor(int argc, char **argv)
{
	int x;
	int rc = 0;
	int locked = 0;
	int mon_lock;
	struct timeval t1, t2;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("monitor");
		}
	}

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	config_set_int(MONITOR_PID_CFG, getpid());

	mon_lock = open(TUP_MONITOR_LOCK, O_RDONLY);
	if(mon_lock < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}
	if(flock(mon_lock, LOCK_EX) < 0) {
		perror("flock");
		rc = -1;
		goto close_monlock;
	}

	gettimeofday(&t1, NULL);

	inot_fd = inotify_init();
	if(inot_fd < 0) {
		perror("inotify_init");
		return -1;
	}

	/* Make sure we're watching the lock before we try to take it
	 * exclusively, since the only way we know to release the lock is if
	 * some other process opens it. We don't want to get in the race
	 * condition of us taking the exclusive lock, then some other process
	 * opens the lock and waits on a shared lock, and then we add a watch
	 * and both sit there staring at each other. Word.
	 */
	obj_wd = inotify_add_watch(inot_fd, TUP_OBJECT_LOCK, IN_OPEN|IN_CLOSE);
	if(obj_wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto close_inot;
	}

	mon_wd = inotify_add_watch(inot_fd, TUP_MONITOR_LOCK, IN_OPEN);
	if(mon_wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto close_inot;
	}

	if(flock(tup_obj_lock(), LOCK_EX) < 0) {
		perror("flock");
		rc = -1;
		goto close_inot;
	}
	locked = 1;

	if(fork() > 0)
		exit(0);

	if(watch_path(".", "") < 0) {
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

			/* If the lock file is opened, assume we are now
			 * locked out. When the file is closed, check to see
			 * if the lock is available again. We can't just
			 * assume the lock is available when the file is closed
			 * because multiple processes can have the lock shared
			 * at once. Also, we can't count the number of opens
			 * and closes because inotify sometimes slurps some
			 * duplicate events.
			 */
			if(e->wd == obj_wd) {
				if(e->mask & IN_OPEN) {
					locked = 0;
					flock(tup_obj_lock(), LOCK_UN);
					DEBUGP("monitor off\n");
				}
				if(e->mask & IN_CLOSE) {
					if(flock(tup_obj_lock(), LOCK_EX | LOCK_NB) < 0) {
						if(errno != EWOULDBLOCK)
							perror("flock");
					} else {
						locked = 1;
						DEBUGP("monitor ON\n");
					}
				}
			} else if(e->wd == mon_wd) {
				goto close_inot;
			} else {
				if(locked)
					handle_event(e);
			}

			offset += sizeof(*e) + e->len;
		}
	}

close_inot:
	close(inot_fd);
close_monlock:
	close(mon_lock);
	config_set_int(MONITOR_PID_CFG, -1);
	return rc;
}

int stop_monitor(int argc, char **argv)
{
	int mon_lock;
	struct timespec ts = {0, 5e6};
	int x;

	if(argc) {}
	if(argv) {}

	mon_lock = open(TUP_MONITOR_LOCK, O_RDONLY);
	if(mon_lock < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}
	close(mon_lock);

	for(x=0; x<25; x++) {
		if(config_get_int(MONITOR_PID_CFG) == -1)
			return 0;
		nanosleep(&ts, NULL);
	}
	fprintf(stderr, "Error: monitor pid never reset.\n");

	return -1;
}

static int watch_path(const char *path, const char *file)
{
	int wd;
	int rc = 0;
	int len;
	uint32_t mask;
	struct flist f = {0, 0, 0};
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
	int cdf = 0;
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

	/* Makefile gets special treatment, since we have to mark the
	 * directory as needing update.
	 */
	if(strcmp(e->name, "Makefile") == 0) {
		if(canonicalize(dc->path, cname, sizeof(cname)) < 0)
			return;
		create_dir_file(cname);
		update_node_flags(cname, TUP_FLAGS_CREATE);
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
			create_name_file(cname);
			cdf = 1;
		}
	}
	if(e->mask & IN_MODIFY || e->mask & IN_ATTRIB) {
		update_node_flags(cname, TUP_FLAGS_MODIFY);
	}
	if(e->mask & IN_DELETE || e->mask & IN_MOVED_FROM) {
		update_node_flags(cname, TUP_FLAGS_DELETE);
		cdf = 1;
	}

	if(cdf) {
		update_create_dir_for_file(cname);
	}

	if(e->mask & IN_IGNORED) {
		dircache_del(dc);
	}
}

static void sighandler(int sig)
{
	if(sig) {}
	config_set_int(MONITOR_PID_CFG, -1);
	/* TODO: gracefully close, or something? */
	exit(0);
}
