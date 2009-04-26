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
#include <sys/resource.h>
#include "dircache.h"
#include "flist.h"
#include "debug.h"
#include "fileio.h"
#include "config.h"
#include "db.h"
#include "lock.h"
#include "memdb.h"
#include "updater.h"

#define MONITOR_TIME_CFG "monitor time"
#define MONITOR_PID_CFG "monitor pid"

struct moved_from_event;
struct monitor_event {
	struct moved_from_event *from_event; /* valid if mask & IN_MOVED_TO */
	struct inotify_event e;
};

struct moved_from_event {
	struct list_head list;
	struct monitor_event m;
};

static int watch_path(tupid_t dt, const char *path, const char *file,
		      int tmpdb);
static int events_queued(void);
static void queue_event(struct inotify_event *e);
static void flush_queue(void);
static int skip_event(struct inotify_event *e);
static int eventcmp(struct inotify_event *e1, struct inotify_event *e2);
static int same_event(struct inotify_event *e1, struct inotify_event *e2);
static int ephemeral_event(struct inotify_event *e);
static int add_from_event(struct inotify_event *e);
static struct moved_from_event *check_from_events(struct inotify_event *e);
static void handle_event(struct monitor_event *m);
static void sighandler(int sig);

static int inot_fd;
static int tup_wd;
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
static struct monitor_event *queue_last_e = NULL;
static sqlite3_int64 mon_time = 0;
static LIST_HEAD(moved_from_list);

int monitor(int argc, char **argv)
{
	int x;
	int rc = 0;
	int locked = 0;
	int mon_lock;
	struct timeval t1, t2;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];
	struct half_entry *he;
	LIST_HEAD(del_list);

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

	tup_wd = inotify_add_watch(inot_fd, TUP_DIR, IN_DELETE);
	if(tup_wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto close_inot;
	}

	/* Make sure we're watching the lock before we release the shared lock
	 * (which will cause any other processes to try to open the object
	 * lock). The only way we know to release the lock is if some other
	 * process opens the object lock.
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

	if(flock(tup_sh_lock(), LOCK_UN) < 0) {
		perror("flock");
		rc = -1;
		goto close_inot;
	}
	locked = 1;

	if(fork() > 0)
		exit(0);

	tup_db_config_set_int(MONITOR_PID_CFG, getpid());
	mon_time = tup_db_config_get_int64(MONITOR_TIME_CFG);

	/* Use tmpdb to store all file objects in a list. We then start to
	 * watch the filesystem, and remove all existing objects from the tmpdb
	 * list. Anything leftover in the tmpdb must have been deleted while
	 * the monitor wasn't looking.
	 */
	if(tup_db_attach_tmpdb() < 0)
		return -1;
	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_files_to_tmpdb() < 0)
		return -1;
	if(tup_db_unflag_tmpdb(DOT_DT) < 0)
		return -1;
	if(tup_db_unflag_tmpdb(VAR_DT) < 0)
		return -1;
	if(watch_path(0, "", ".", 1) < 0) {
		rc = -1;
		goto close_inot;
	}
	if(tup_db_get_all_in_tmpdb(&del_list) < 0)
		return -1;
	while(!list_empty(&del_list)) {
		he = list_entry(del_list.next, struct half_entry, list);
		if(tup_del_id(he->tupid, he->dt, he->sym, he->type) < 0)
			return -1;
		list_del(&he->list);
		free(he);
	}
	mon_time = time(NULL);
	tup_db_config_set_int64(MONITOR_TIME_CFG, mon_time);
	if(tup_db_commit() < 0)
		return -1;
	if(tup_db_detach_tmpdb() < 0)
		return -1;

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	while((x = read(inot_fd, buf, sizeof(buf))) > 0) {
		struct inotify_event *e;
		int offset = 0;

		for(offset = 0; offset < x; offset += sizeof(*e) + e->len) {
			e = (void*)((char*)buf + offset);

			DEBUGP("%c[%i: %i]: '%s' %08x [%i, %i]\n", locked? 'E' : 'e', e->wd,
			       sizeof(*e) + e->len, e->len ? e->name : "", e->mask, tup_wd, obj_wd);
			/* If the object lock file is opened, assume we are now
			 * locked out. We take the tri lock before releasing
			 * the object lock, so we can make sure we are the
			 * first to get the object lock again when it is
			 * available (anyone else will be stuck waiting on the
			 * shared lock...suckers!) When the lock file is then
			 * closed, we take the object lock, then release the
			 * tri lock. Whoever had the object lock is waiting on
			 * the tri lock before they release the shared lock.
			 * So, we at least get a quick run in with locked
			 * access (and can process any events between the
			 * locks) before someone else gets the shared lock and
			 * opens the object lock, causing us to go offline
			 * again.
			 */
			if(e->wd == tup_wd) {
				if(e->len && strncmp(e->name, "db-", 3) == 0)
					continue;
				printf("tup monitor: .tup file '%s' deleted - shutting down.\n", e->len ? e->name : "");
				goto close_inot;
			} else if(e->wd == obj_wd) {
				if((e->mask & IN_OPEN) && locked) {
					flush_queue();
					locked = 0;
					if(flock(tup_tri_lock(), LOCK_EX) < 0) {
						perror("flock");
						goto close_inot;
					}
					if(flock(tup_obj_lock(), LOCK_UN) < 0) {
						perror("flock");
						goto close_inot;
					}
					DEBUGP("monitor off\n");
				}
				if((e->mask & IN_CLOSE) && !locked) {
					if(flock(tup_obj_lock(), LOCK_EX) < 0) {
						perror("flock");
						goto close_inot;
					}
					if(flock(tup_tri_lock(), LOCK_UN) < 0) {
						perror("flock");
						goto close_inot;
					}
					locked = 1;
					DEBUGP("monitor ON\n");

					/* Make sure we update our time, since
					 * if the updater ran then some files
					 * will have newer timestamps.
					 */
					mon_time = time(NULL);
					tup_db_config_set_int64(MONITOR_TIME_CFG, mon_time);
				}
			} else if(e->wd == mon_wd) {
				goto close_inot;
			} else {
				if(locked)
					queue_event(e);
			}
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
	return rc;
}

int stop_monitor(int argc, char **argv)
{
	int mon_lock;
	int pid;

	if(argc) {}
	if(argv) {}

	pid = tup_db_config_get_int(MONITOR_PID_CFG);
	if(pid < 0) {
		printf("No monitor process to kill (pid < 0)\n");
		return 0;
	}
	/* Just using getpriority() to see if the monitor process is alive. */
	errno = 0;
	if(getpriority(PRIO_PROCESS, pid) == -1 && errno == ESRCH) {
		printf("Monitor pid %i doesn't exist anymore.\n", pid);
	} else {
		printf("Shutting down monitor.\n");
	}
	mon_lock = open(TUP_MONITOR_LOCK, O_RDONLY);
	if(mon_lock < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}
	close(mon_lock);
	tup_db_config_set_int(MONITOR_PID_CFG, -1);

	return 0;
}

static int watch_path(tupid_t dt, const char *path, const char *file, int tmpdb)
{
	int wd;
	uint32_t mask;
	struct flist f = {0, 0, 0};
	struct stat buf;
	int rc = -1;
	char *fullpath;
	int curfd;
	tupid_t newdt;

	curfd = open(".", O_RDONLY);
	if(curfd < 0) {
		perror(".");
		return -1;
	}
	if(path[0] != 0)
		chdir(path);

	if(lstat(file, &buf) != 0) {
		fprintf(stderr, "Hey wtf\n");
		perror(file);
		goto out_close;
	}

	if(S_ISREG(buf.st_mode)) {
		struct db_node dbn;

		if(tup_db_select_dbn(dt, file, &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			if(create_name_file(dt, file) < 0)
				goto out_close;
		} else {
			if(tmpdb) {
				if(tup_db_unflag_tmpdb(dbn.tupid) < 0)
					goto out_close;
			}
			if(mon_time != -1 && buf.st_mtime > mon_time) {
				if(tup_db_add_modify_list(dbn.tupid) < 0)
					goto out_close;
			}
		}
		rc = 0;
		goto out_close;
	} else if(S_ISLNK(buf.st_mode)) {
		tupid_t tupid;

		tupid = update_symlink_file(dt, file);
		if(tupid < 0)
			goto out_close;
		if(tmpdb) {
			if(tup_db_unflag_tmpdb(tupid) < 0)
				goto out_close;
			if(mon_time != -1 && buf.st_mtime > mon_time) {
				if(tup_db_add_modify_list(tupid) < 0)
					goto out_close;
			}
		}
		rc = 0;
		goto out_close;
	} else if(S_ISDIR(buf.st_mode)) {
		newdt = create_dir_file(dt, file);
		if(tmpdb) {
			if(tup_db_unflag_tmpdb(newdt) < 0)
				goto out_close;
		}
	} else {
		fprintf(stderr, "Error: File '%s' is not regular nor a dir?\n",
			file);
		goto out_close;
	}

	DEBUGP("add watch: '%s'\n", file);

	mask = IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVE;
	wd = inotify_add_watch(inot_fd, file, mask);
	if(wd < 0) {
		perror("inotify_add_watch");
		goto out_close;
	}

	if(path[0] == 0) {
		fullpath = strdup(".");
	} else if(strcmp(path, ".") == 0) {
		fullpath = strdup(file);
	} else {
		if(asprintf(&fullpath, "%s/%s", path, file) < 0) {
			perror("asprintf");
			goto out_close;
		}
	}
	if(!fullpath) {
		perror("Unable to allocate space for path.\n");
		goto out_close;
	}
	dircache_add(&mdb, wd, fullpath, newdt);
	/* dircache assumes ownership of fullpath */

	fchdir(curfd);
	close(curfd);

	flist_foreach(&f, fullpath) {
		if(f.filename[0] == '.')
			continue;
		if(watch_path(newdt, fullpath, f.filename, tmpdb) < 0)
			goto out_close;
	}
	return 0;

out_close:
	fchdir(curfd);
	close(curfd);

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
	struct moved_from_event *mfe = NULL;
	struct monitor_event *m;

	if(skip_event(e))
		return;
	if(queue_last_e && eventcmp(&queue_last_e->e, e) == 0)
		return;
	if(ephemeral_event(e) == 0)
		return;
	if(e->mask & IN_MOVED_FROM) {
		add_from_event(e);
		return;
	}

	if(e->mask & IN_MOVED_TO) {
		mfe = check_from_events(e);
		/* It's possible mfe is still NULL here. For example, the
		 * IN_MOVED_FROM event may have been destroyed because it was
		 * ephemeral. In this case, we just leave IN_MOVED_TO as if it
		 * is just a single file-creation event.
		 */
	}
	DEBUGP("Queue[%i]: '%s' %08x\n",
	       sizeof(*e) + e->len, e->len ? e->name : "", e->mask);

	new_start = queue_end;
	new_end = new_start + sizeof(*m) + e->len;
	if(new_end >= (signed)sizeof(queue_buf)) {
		flush_queue();
		new_start = queue_end;
		new_end = new_start + sizeof(*m) + e->len;
		if(new_end >= (signed)sizeof(queue_buf)) {
			fprintf(stderr, "Error: Event dropped\n");
			return;
		}
	}

	queue_last_e = (struct monitor_event*)&queue_buf[new_start];
	m = queue_last_e;

	memcpy(&m->e, e, sizeof(*e));
	memcpy(m->e.name, e->name, e->len);
	m->from_event = mfe;

	queue_end += sizeof(*m) + e->len;
}

static void flush_queue(void)
{
	int events_handled = 0;

	tup_db_begin();
	while(queue_start < queue_end) {
		struct monitor_event *m;
		struct inotify_event *e;

		m = (struct monitor_event*)&queue_buf[queue_start];
		e = &m->e;
		DEBUGP("Handle[%i]: '%s' %08x\n",
		       sizeof(*m) + e->len, e->len ? e->name : "", e->mask);
		if(e->mask != 0)
			handle_event(m);
		queue_start += sizeof(*m) + e->len;
		events_handled = 1;
	}
	if(events_handled) {
		mon_time = time(NULL);
		tup_db_config_set_int64(MONITOR_TIME_CFG, mon_time);
	}
	tup_db_commit();

	queue_start = 0;
	queue_end = 0;
	queue_last_e = NULL;
	if(events_handled && tup_db_config_get_int("autoupdate") == 1) {
		/* This runs in a separate process (as opposed to just calling
		 * updater() directly) so it can properly get the lock from us
		 * (the monitor) and flush the queue correctly. Otherwise files
		 * touched by the updater will be caught by us, which is
		 * annoying.
		 */
		int pid = fork();
		if(pid < 0) {
			perror("fork");
			return;
		}
		if(pid == 0) {
			if(tup_lock_init() < 0)
				exit(1);
			updater(1, NULL, 0);
			tup_db_config_set_int(AUTOUPDATE_PID, -1);
			tup_lock_exit();
			exit(0);
		} else {
			tup_db_config_set_int(AUTOUPDATE_PID, pid);
		}
	}
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
	struct monitor_event *m;
	struct inotify_event *qe;
	int create_and_delete = 0;
	int rc = -1;
	int newflags = IN_CREATE | IN_MOVED_TO;
	int delflags = IN_DELETE | IN_MOVED_FROM;

	if(!(e->mask & (newflags | delflags))) {
		return -1;
	}

	for(x=queue_start; x<queue_end;) {
		m = (struct monitor_event*)&queue_buf[x];
		qe = &m->e;

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

		x += sizeof(*m) + qe->len;
	}

	return rc;
}

static int add_from_event(struct inotify_event *e)
{
	struct moved_from_event *mfe;

	mfe = malloc(sizeof *mfe + e->len);
	if(!mfe) {
		perror("malloc");
		return -1;
	}

	memcpy(&mfe->m.e, e, sizeof(*e));
	memcpy(mfe->m.e.name, e->name, e->len);

	mfe->m.from_event = NULL;
	list_add(&mfe->list, &moved_from_list);

	return 0;
}

static struct moved_from_event *check_from_events(struct inotify_event *e)
{
	struct moved_from_event *mfe;
	list_for_each_entry(mfe, &moved_from_list, list) {
		if(mfe->m.e.cookie == e->cookie) {
			return mfe;
		}
	}
	return NULL;
}

static void handle_event(struct monitor_event *m)
{
	struct dircache *dc;

	dc = dircache_lookup(&mdb, m->e.wd);
	if(!dc) {
		fprintf(stderr, "Error: dircache entry not found for wd %i\n",
			m->e.wd);
		return;
	}

	if(m->e.mask & IN_IGNORED) {
		dircache_del(&mdb, dc);
		return;
	}

	if(m->e.mask & IN_MOVED_TO && m->from_event) {
		struct moved_from_event *mfe = m->from_event;
		struct dircache *from_dc;

		from_dc = dircache_lookup(&mdb, mfe->m.e.wd);
		if(!from_dc) {
			fprintf(stderr, "Error: dircache entry not found for from event wd %i\n", mfe->m.e.wd);
			return;
		}
		if(m->e.mask & IN_ISDIR) {
			struct db_node dbn;

			if(tup_db_select_dbn(from_dc->dt, mfe->m.e.name, &dbn) < 0)
				return;
			if(dbn.tupid < 0)
				return;
			if(tup_db_change_node(dbn.tupid, m->e.name, dc->dt) < 0)
				return;
			if(tup_db_modify_dir(dbn.tupid) < 0)
				return;
			watch_path(dc->dt, dc->path, m->e.name, 0);
		} else {
			tup_file_mod(dc->dt, m->e.name);
			tup_file_del(from_dc->dt, mfe->m.e.name);
		}
		list_del(&mfe->list);
		free(mfe);
		return;
	}

	/* Handle IN_MOVED_TO events without corresponding from events as if
	 * they are IN_CREATE.
	 */
	if(m->e.mask & IN_CREATE || m->e.mask & IN_MOVED_TO) {
		watch_path(dc->dt, dc->path, m->e.name, 0);
		return;
	}
	if(m->e.mask & IN_MODIFY || m->e.mask & IN_ATTRIB) {
		tup_file_mod(dc->dt, m->e.name);
	}
	if(m->e.mask & IN_DELETE) {
		tup_file_del(dc->dt, m->e.name);
	}
}

static void sighandler(int sig)
{
	if(sig) {}
	tup_db_config_set_int(MONITOR_PID_CFG, -1);
	/* TODO: gracefully close, or something? */
	exit(0);
}
