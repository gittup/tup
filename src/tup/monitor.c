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
#include <sys/select.h>
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
#include "memdb.h"

#define MONITOR_PID_CFG "monitor pid"

static int watch_path(const char *path, const char *file);
static int events_queued(void);
static void queue_event(struct inotify_event *e);
static void flush_queue(void);
static int skip_event(struct inotify_event *e);
static int eventcmp(struct inotify_event *e1, struct inotify_event *e2);
static int same_event(struct inotify_event *e1, struct inotify_event *e2);
static int ephemeral_event(struct inotify_event *e);
static void handle_event(struct inotify_event *e);
static void check_deletion(struct inotify_event *e);
static void sighandler(int sig);

static int inot_fd;
static int obj_wd;
static int mon_wd;
static struct memdb mdb;
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};
static char queue_buf[(sizeof(struct inotify_event) + 16) * 1024];
static int queue_start = 0;
static int queue_end = 0;
static struct inotify_event *queue_last_e = NULL;

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

	if(memdb_init(&mdb) < 0)
		return -1;

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	tup_db_config_set_int(MONITOR_PID_CFG, getpid());

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

	tup_db_begin();
	if(watch_path(".", "") < 0) {
		rc = -1;
		goto close_inot;
	}
	tup_db_commit();

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	while((x = read(inot_fd, buf, sizeof(buf))) > 0) {
		int offset = 0;

		while(offset < x) {
			struct inotify_event *e = (void*)((char*)buf + offset);

			if(e->mask & IN_ISDIR && (e->mask & IN_DELETE ||
						  e->mask & IN_MOVED_FROM)) {
				check_deletion(e);
			}

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
				if((e->mask & IN_OPEN) && locked) {
					flush_queue();
					locked = 0;
					flock(tup_obj_lock(), LOCK_UN);
					DEBUGP("monitor off\n");
				}
				if((e->mask & IN_CLOSE) && !locked) {
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
					queue_event(e);
			}

			offset += sizeof(*e) + e->len;
		}

		if(events_queued()) {
			struct timeval tv = {0, 100000};
			int ret;
			fd_set rfds;

			FD_ZERO(&rfds);
			FD_SET(inot_fd, &rfds);
			ret = select(inot_fd+1, &rfds, NULL, NULL, &tv);
			if(ret == 0) {
				/* Timeout, flush queue */
				flush_queue();
			}
		}
	}

close_inot:
	close(inot_fd);
close_monlock:
	close(mon_lock);
	tup_db_config_set_int(MONITOR_PID_CFG, -1);
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
		if(tup_db_config_get_int(MONITOR_PID_CFG) == -1)
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
	int lastslash;
	tupid_t dt;

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
	len = canonicalize_string(fullpath, len, &lastslash);
	if(len < 0)
		goto out_free;

	if(stat(fullpath, &buf) != 0) {
		perror(fullpath);
		rc = -1;
		goto out_free;
	}

	if(S_ISREG(buf.st_mode)) {
		/* TODO: Re-use tup_file_mod() somehow? */
		fullpath[lastslash] = 0;
		dt = create_dir_file(fullpath);
		if(dt < 0) {
			rc = -1;
			goto out_free;
		}
		if(tup_db_select_node(dt, file) < 0)
			if(tup_db_set_flags_by_id(dt, TUP_FLAGS_CREATE) < 0) {
				rc = -1;
				goto out_free;
			}
		create_name_file(dt, file);
		goto out_free;
	}
	if(S_ISDIR(buf.st_mode)) {
		dt = create_dir_file(fullpath);
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
	dircache_add(&mdb, wd, fullpath, dt);
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

static int events_queued(void)
{
	return queue_start != queue_end;
}

static void queue_event(struct inotify_event *e)
{
	int new_start;
	int new_end;

	if(skip_event(e))
		return;
	if(eventcmp(queue_last_e, e) == 0)
		return;
	if(ephemeral_event(e) == 0)
		return;
	DEBUGP("Queue[%i]: '%s' %08x\n",
	       sizeof(*e) + e->len, e->len ? e->name : "", e->mask);

	new_start = queue_end;
	new_end = new_start + sizeof(*e) + e->len;
	if(new_end >= (signed)sizeof(queue_buf)) {
		fprintf(stderr, "Error: Event dropped\n");
		return;
	}

	queue_last_e = (struct inotify_event*)&queue_buf[new_start];

	memcpy(&queue_buf[new_start], e, sizeof(*e));
	memcpy(&queue_buf[new_start + sizeof(*e)], e->name, e->len);

	queue_end += sizeof(*e) + e->len;
}

static void flush_queue(void)
{
	tup_db_begin();
	while(queue_start < queue_end) {
		struct inotify_event *e;

		e = (struct inotify_event*)&queue_buf[queue_start];
		DEBUGP("Handle[%i]: '%s' %08x\n",
		       sizeof(*e) + e->len, e->len ? e->name : "", e->mask);
		if(e->mask != 0)
			handle_event(e);
		queue_start += sizeof(*e) + e->len;
	}
	tup_db_commit();

	queue_start = 0;
	queue_end = 0;
	queue_last_e = NULL;
}

static int skip_event(struct inotify_event *e)
{
	/* Skip hidden files */
	if(e->len && e->name[0] == '.')
		return 1;
	return 0;
}

static int eventcmp(struct inotify_event *e1, struct inotify_event *e2)
{
	/* Checks if events are identical in every way. See also same_event() */
	if(!e1 || !e2)
		return -1;
	if(memcmp(e1, e2, sizeof(struct inotify_event)) != 0)
		return -1;
	if(e1->len && strcmp(e1->name, e2->name) != 0)
		return -1;
	return 0;
}

static int same_event(struct inotify_event *e1, struct inotify_event *e2)
{
	/* Checks if events are on the same file, but may have a different mask
	 * and/or cookie. See also eventcmp()
	 */
	if(!e1 || !e2)
		return -1;
	if(e1->wd != e2->wd || e1->len != e2->len)
		return -1;
	if(e1->len && strcmp(e1->name, e2->name) != 0)
		return -1;
	return 0;
}

static int ephemeral_event(struct inotify_event *e)
{
	int x;
	struct inotify_event *qe;
	int create_and_delete = 0;
	int rc = -1;
	int newflags = IN_CREATE | IN_MOVED_TO;
	int delflags = IN_DELETE | IN_MOVED_FROM;

	if(!(e->mask & (newflags | delflags))) {
		return -1;
	}

	for(x=queue_start; x<queue_end;) {
		qe = (struct inotify_event*)&queue_buf[x];

		if(same_event(qe, e) == 0) {
			if(qe->mask & newflags && e->mask & delflags) {
				/* Previously the file was created, now it's
				 * destroyed, so we delete all record of it.
				 */
				rc = 0;
				qe->mask = 0;
				create_and_delete = 1;
			} else if(qe->mask & delflags && e->mask & newflags) {
				/* The file was previously deleted and now it's
				 * recreated. Remove almost all record of it,
				 * except we set the latest event to be
				 * 'modified', since we effectively just
				 * updated the file.
				 *
				 * Note we don't set rc here since we want
				 * to still write e to the queue.
				 */
				qe->mask = 0;
				e->mask = IN_MODIFY;
				create_and_delete = 1;
			} else if(create_and_delete) {
				/* Delete any events about a file in between
				 * when it was created and subsequently
				 * deleted, or deleted and subsequently
				 * re-created.
				 */
				qe->mask = 0;
			}
		}

		x += sizeof(*qe) + qe->len;
	}

	return rc;
}

static void handle_event(struct inotify_event *e)
{
	struct dircache *dc;
	int flags = 0;
	printf("event: wd=%i, name='%s' mask=%08x\n", e->wd, e->len ? e->name : "", e->mask);

	dc = dircache_lookup(&mdb, e->wd);
	if(!dc) {
		fprintf(stderr, "Error: dircache entry not found for wd %i\n",
			e->wd);
		return;
	}

	if(e->mask & IN_IGNORED) {
		dircache_del(&mdb, dc);
		return;
	}

	if(e->mask & IN_CREATE || e->mask & IN_MOVED_TO) {
		if(e->mask & IN_ISDIR) {
			watch_path(dc->path, e->name);
			return;
		}
		flags = TUP_FLAGS_MODIFY;
	}
	if(e->mask & IN_MODIFY || e->mask & IN_ATTRIB) {
		flags = TUP_FLAGS_MODIFY;
	}
	if(e->mask & IN_DELETE || e->mask & IN_MOVED_FROM) {
		flags = TUP_FLAGS_DELETE;
	}

	if(e->mask & IN_ISDIR) {
		static char cname[PATH_MAX];
		int len;

		len = canonicalize2(dc->path, e->name, cname, sizeof(cname), NULL);
		if(len < 0)
			return;
		tup_file_mod(0, cname, flags);
	} else {
		tup_file_mod(dc->dt, e->name, flags);
	}
}

static void check_deletion(struct inotify_event *e)
{
	static char cname[PATH_MAX];
	struct dircache *dc;
	int len;
	DEBUGP("check deletion: wd=%i, name='%s'\n", e->wd, e->name);

	dc = dircache_lookup(&mdb, e->wd);
	if(!dc) {
		fprintf(stderr, "Error: dircache entry not found for wd %i\n",
			e->wd);
		return;
	}

	len = canonicalize2(dc->path, e->name, cname, sizeof(cname), NULL);
	if(len < 0)
		return;

	tup_db_delete_dir(cname);
}

static void sighandler(int sig)
{
	if(sig) {}
	tup_db_config_set_int(MONITOR_PID_CFG, -1);
	/* TODO: gracefully close, or something? */
	exit(0);
}
