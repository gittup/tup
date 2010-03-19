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

#define _ATFILE_SOURCE
#include "tup/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <pthread.h>
#include "tup/dircache.h"
#include "tup/debug.h"
#include "tup/fileio.h"
#include "tup/config.h"
#include "tup/db.h"
#include "tup/lock.h"
#include "tup/updater.h"
#include "tup/path.h"
#include "tup/entry.h"
#include "tup/fslurp.h"
#include "linux/rbtree.h"

#define MONITOR_LOOP_RETRY -2

struct moved_from_event;
struct monitor_event {
	/* from_event is valid if mask is IN_MOVED_TO or IN_MOVED_FROM */
	struct moved_from_event *from_event;
	struct inotify_event e;
};

struct moved_from_event {
	struct list_head list;
	struct monitor_event *m;
};

static int monitor_set_pid(int pid);
static int monitor_loop(void);
static int wp_callback(tupid_t newdt, int dfd, const char *file);
static int events_queued(void);
static int queue_event(struct inotify_event *e);
static int flush_queue(void);
static void *wait_thread(void *arg);
static int skip_event(struct inotify_event *e);
static int eventcmp(struct inotify_event *e1, struct inotify_event *e2);
static int same_event(struct inotify_event *e1, struct inotify_event *e2);
static int ephemeral_event(struct inotify_event *e);
static struct moved_from_event *add_from_event(struct monitor_event *m);
static struct moved_from_event *check_from_events(struct inotify_event *e);
static void monitor_rmdir_cb(tupid_t dt);
static int handle_event(struct monitor_event *m);
static void pinotify(void);
static void sighandler(int sig);

static int inot_fd;
static int tup_wd;
static int obj_wd;
static int mon_wd;
static struct dircache_root droot;
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};
static char queue_buf[(sizeof(struct inotify_event) + 16) * 1024];
static int queue_start = 0;
static int queue_end = 0;
static struct monitor_event *queue_last_e = NULL;
static LIST_HEAD(moved_from_list);

int monitor_supported(void)
{
	return 0;
}

int monitor(int argc, char **argv)
{
	int x;
	int rc = 0;
	int mon_fd;

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("monitor");
		}
	}

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);

	mon_fd = openat(tup_top_fd(), TUP_MONITOR_LOCK, O_RDWR);
	if(mon_fd < 0) {
		perror(TUP_MONITOR_LOCK);
		return -1;
	}

	inot_fd = inotify_init();
	if(inot_fd < 0) {
		perror("inotify_init");
		return -1;
	}

	tup_wd = inotify_add_watch(inot_fd, TUP_DIR, IN_DELETE);
	if(tup_wd < 0) {
		pinotify();
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
		pinotify();
		rc = -1;
		goto close_inot;
	}

	mon_wd = inotify_add_watch(inot_fd, TUP_MONITOR_LOCK, IN_OPEN);
	if(mon_wd < 0) {
		pinotify();
		rc = -1;
		goto close_inot;
	}

	if(fork() > 0) {
		/* Remove our object lock, then wait for the child process to get
		 * it.
		 */
		tup_unflock(tup_obj_lock());
		if(tup_wait_flock(tup_obj_lock()) < 0)
			exit(1);
		exit(0);
	}

	/* Child must re-acquire the object lock, since we lost it at the
	 * fork
	 */
	if(tup_flock(tup_obj_lock()) < 0) {
		rc = -1;
		goto close_inot;
	}

	if(monitor_set_pid(getpid()) < 0) {
		rc = -1;
		goto close_inot;
	}

	dircache_init(&droot);
	tup_register_rmdir_callback(monitor_rmdir_cb);

	do {
		rc = monitor_loop();
		if(rc == MONITOR_LOOP_RETRY) {
			struct rb_node *rbn;
			struct timeval tv = {0, 0};
			int ret;
			fd_set rfds;

			/* Need to clear out all saved structures (the dircache
			 * and tup_entries), then shut the monitor off before
			 * turning it back on. If there is a waiting 'tup upd'
			 * it will get the lock and update in scan mode before
			 * we return from tup_lock_init(). Then we should be
			 * good to go.
			 */
			while((rbn = rb_first(&droot.wd_tree)) != NULL) {
				struct tupid_tree *tt = rb_entry(rbn, struct tupid_tree, rbn);
				struct dircache *dc = container_of(tt, struct dircache, wd_node);
				inotify_rm_watch(inot_fd, dc->wd_node.tupid);
				dircache_del(&droot, dc);
			}

			if(tup_entry_clear() < 0)
				return -1;
			if(monitor_set_pid(-1) < 0)
				return -1;
			tup_lock_close();

			if(fchdir(tup_top_fd()) < 0) {
				perror("fchdir tup_top");
				return -1;
			}
			if(tup_lock_init() < 0)
				return -1;

			/* Flush the inotify queue */
			while(1) {
				char buf[1024];
				FD_ZERO(&rfds);
				FD_SET(inot_fd, &rfds);
				ret = select(inot_fd+1, &rfds, NULL, NULL, &tv);
				if(ret < 0) {
					perror("select");
					return -1;
				}
				if(ret == 0)
					break;
				if(read(inot_fd, buf, sizeof(buf)) < 0) {
					perror("read");
					return -1;
				}
			}

			if(tup_unflock(tup_sh_lock()) < 0) {
				return -1;
			}
			if(monitor_set_pid(getpid()) < 0)
				return -1;
		}
	} while(rc == MONITOR_LOOP_RETRY);
	monitor_set_pid(-1);

close_inot:
	close(inot_fd);
	close(mon_fd);
	return rc;
}

static int monitor_set_pid(int pid)
{
	char buf[32];
	int len;
	int fd;

	fd = openat(tup_top_fd(), MONITOR_PID_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(fd < 0) {
		perror(MONITOR_PID_FILE);
		return -1;
	}
	if(tup_flock(fd) < 0) {
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "%i", pid);
	if(len >= (signed)sizeof(buf)) {
		fprintf(stderr, "Buf is sized too small in monitor_set_pid\n");
		return -1;
	}
	if(write(fd, buf, len) < 0) {
		perror("write");
		return -1;
	}
	if(ftruncate(fd, len) < 0) {
		perror("ftruncate");
		return -1;
	}
	if(tup_unflock(fd) < 0) {
		return -1;
	}
	close(fd);
	return 0;
}

int monitor_get_pid(void)
{
	struct buf b;
	int fd;
	int rc = -1;

	fd = openat(tup_top_fd(), MONITOR_PID_FILE, O_RDWR, 0666);
	if(fd < 0) {
		if(errno != ENOENT) {
			perror(MONITOR_PID_FILE);
			return -1;
		}
		return -1;
	}
	if(tup_flock(fd) < 0) {
		return -1;
	}
	if(fslurp_null(fd, &b) < 0) {
		goto out;
	}

	if(b.len > 0) {
		rc = strtol(b.s, NULL, 0);
	}
	free(b.s);
out:
	if(tup_unflock(fd) < 0) {
		return -1;
	}
	close(fd);

	if(rc > 0) {
		/* Just using getpriority() to see if the monitor process is
		 * alive.
		 */
		errno = 0;
		if(getpriority(PRIO_PROCESS, rc) == -1 && errno == ESRCH) {
			printf("Monitor pid %i doesn't exist anymore.\n", rc);
			monitor_set_pid(-1);
			rc = -1;
		}
	}
	return rc;
}

static int monitor_loop(void)
{
	int x;
	int rc;
	struct timeval t1, t2;
	static char buf[(sizeof(struct inotify_event) + 16) * 4096];
	struct rb_root scan_tree = RB_ROOT;
	int locked = 1;

	gettimeofday(&t1, NULL);

	if(tup_db_scan_begin(&scan_tree) < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", &scan_tree, wp_callback) < 0)
		return -1;
	if(tup_db_scan_end(&scan_tree) < 0)
		return -1;

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	do {
		struct inotify_event *e;
		int offset = 0;

		x = read(inot_fd, buf, sizeof(buf));
		if(x < 0) {
			if(errno == EINTR) {
				continue;
			} else {
				perror("read");
				return -1;
			}
		}

		for(offset = 0; offset < x; offset += sizeof(*e) + e->len) {
			e = (void*)((char*)buf + offset);

			DEBUGP("%c[%i: %li]: '%s' %08x [%i, %i]\n", locked? 'E' : 'e', e->wd,
			       (long)sizeof(*e) + e->len, e->len ? e->name : "", e->mask, tup_wd, obj_wd);
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
				return 0;
			} else if(e->wd == obj_wd) {
				if((e->mask & IN_OPEN) && locked) {
					rc = flush_queue();
					if(rc < 0)
						return rc;
					locked = 0;
					if(tup_flock(tup_tri_lock()) < 0) {
						return -1;
					}
					if(tup_unflock(tup_obj_lock()) < 0) {
						return -1;
					}
					DEBUGP("monitor off\n");
				}
				if((e->mask & IN_CLOSE) && !locked) {
					if(tup_flock(tup_obj_lock()) < 0) {
						return -1;
					}
					if(tup_unflock(tup_tri_lock()) < 0) {
						return -1;
					}
					locked = 1;
					DEBUGP("monitor ON\n");
				}
			} else if(e->wd == mon_wd) {
				return 0;
			} else {
				if(locked) {
					rc = queue_event(e);
					if(rc < 0)
						return rc;
				}
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
				rc = flush_queue();
				if(rc < 0)
					return rc;
			}
		}
	} while(1);

	return 0;
}

int stop_monitor(void)
{
	int pid;

	if(find_tup_dir() < 0)
		return -1;

	pid = monitor_get_pid();
	if(pid < 0) {
		printf("No monitor process to kill (pid < 0)\n");
		return -1;
	}
	printf("Shutting down monitor.\n");
	if(kill(pid, SIGKILL) < 0) {
		perror("kill");
		return -1;
	}
	monitor_set_pid(-1);

	return 0;
}

static int wp_callback(tupid_t newdt, int dfd, const char *file)
{
	int wd;
	uint32_t mask;

	DEBUGP("add watch: '%s'\n", file);

	if(fchdir(dfd) < 0) {
		perror("fchdir");
		return -1;
	}
	mask = IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVE;
	wd = inotify_add_watch(inot_fd, file, mask);
	if(wd < 0) {
		pinotify();
		return -1;
	}

	dircache_add(&droot, wd, newdt);
	return 0;
}

static int events_queued(void)
{
	return queue_start != queue_end;
}

static int queue_event(struct inotify_event *e)
{
	int new_start;
	int new_end;
	struct moved_from_event *mfe = NULL;
	struct monitor_event *m;
	int rc;

	if(skip_event(e))
		return 0;
	if(queue_last_e && eventcmp(&queue_last_e->e, e) == 0)
		return 0;
	if(ephemeral_event(e) == 0)
		return 0;

	if(e->mask & IN_MOVED_TO) {
		mfe = check_from_events(e);
		/* It's possible mfe is still NULL here. For example, the
		 * IN_MOVED_FROM event may have been destroyed because it was
		 * ephemeral. In this case, we just leave IN_MOVED_TO as if it
		 * is just a single file-creation event.
		 */
	}
	DEBUGP("[33mQueue[%li]: %i, '%s' %08x[0m\n",
	       (long)sizeof(*e)+e->len, e->wd, e->len ? e->name : "", e->mask);

	new_start = queue_end;
	new_end = new_start + sizeof(*m) + e->len;
	if(new_end >= (signed)sizeof(queue_buf)) {
		rc = flush_queue();
		if(rc < 0)
			return rc;
		new_start = queue_end;
		new_end = new_start + sizeof(*m) + e->len;
		if(new_end >= (signed)sizeof(queue_buf)) {
			fprintf(stderr, "Error: Event dropped\n");
			return MONITOR_LOOP_RETRY;
		}
	}

	queue_last_e = (struct monitor_event*)&queue_buf[new_start];
	m = queue_last_e;

	memcpy(&m->e, e, sizeof(*e));
	memcpy(m->e.name, e->name, e->len);
	m->from_event = mfe;

	if(e->mask & IN_MOVED_FROM) {
		m->from_event = add_from_event(m);
	}

	queue_end += sizeof(*m) + e->len;
	return 0;
}

static int flush_queue(void)
{
	int events_handled = 0;
	int overflow = 0;

	tup_db_begin();
	while(queue_start < queue_end) {
		struct monitor_event *m;
		struct inotify_event *e;

		m = (struct monitor_event*)&queue_buf[queue_start];
		e = &m->e;
		DEBUGP("Handle[%li]: '%s' %08x\n",
		       (long)sizeof(*m) + e->len, e->len ? e->name : "", e->mask);
		if(e->wd == -1) {
			overflow = 1;
		} else {
			if(e->mask != 0) {
				if(handle_event(m) < 0) {
					tup_db_rollback();
					return -1;
				}
			}
		}
		queue_start += sizeof(*m) + e->len;
		events_handled = 1;
	}
	tup_db_commit();

	queue_start = 0;
	queue_end = 0;
	queue_last_e = NULL;

	if(overflow) {
		fprintf(stderr, "Received overflow event - restarting monitor.\n");
		return MONITOR_LOOP_RETRY;
	}

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
			return -1;
		}
		if(pid == 0) {
			if(fchdir(tup_top_fd()) < 0) {
				perror("fchdir tup_top");
				exit(1);
			}
			if(tup_lock_init() < 0)
				exit(1);
			updater(1, NULL, 0);
			tup_db_config_set_int(AUTOUPDATE_PID, -1);
			tup_lock_exit();
			exit(0);
		} else {
			int *newpid;
			pthread_t tid;

			newpid = malloc(sizeof(int));
			if(!newpid) {
				perror("malloc");
				return -1;
			}
			*newpid = pid;
			if(pthread_create(&tid, NULL, wait_thread, (void*)newpid) < 0) {
				perror("pthread_create");
				return -1;
			}
			if(pthread_detach(tid) < 0) {
				perror("pthread_detach");
				return -1;
			}
			tup_db_config_set_int(AUTOUPDATE_PID, pid);
		}
	}
	return 0;
}

static void *wait_thread(void *arg)
{
	/* Apparently setting SIGCHLD to SIG_IGN isn't particularly portable,
	 * so I use this stupid thread instead. Maybe there's a better way.
	 */
	int *pid = (int*)arg;

	if(waitpid(*pid, NULL, 0) < 0) {
		perror("waitpid");
	}
	free(pid);
	return NULL;
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

	/* Don't want to remove events on directories, since we will need to
	 * make sure we keep track of the details of all subdirectories and
	 * such (ie: if a directory is moved out of the way and a new directory
	 * is moved in its place, we don't want to ignore that). See t7034.
	 */
	if(e->mask & IN_ISDIR)
		return -1;

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

static struct moved_from_event *add_from_event(struct monitor_event *m)
{
	struct moved_from_event *mfe;

	mfe = malloc(sizeof *mfe);
	if(!mfe) {
		perror("malloc");
		return NULL;
	}

	mfe->m = m;
	/* The mfe is removed from the list in either check_from_events, or in
	 * handle_event if this event is never claimed.
	 */
	list_add(&mfe->list, &moved_from_list);

	return mfe;
}

static struct moved_from_event *check_from_events(struct inotify_event *e)
{
	struct moved_from_event *mfe;
	list_for_each_entry(mfe, &moved_from_list, list) {
		if(mfe->m->e.cookie == e->cookie) {
			mfe->m->e.mask = 0;
			list_del(&mfe->list);
			return mfe;
		}
	}
	return NULL;
}

static void monitor_rmdir_cb(tupid_t dt)
{
	struct dircache *dc;

	dc = dircache_lookup_dt(&droot, dt);
	if(dc) {
		inotify_rm_watch(inot_fd, dc->wd_node.tupid);
		dircache_del(&droot, dc);
	}
}

static int handle_event(struct monitor_event *m)
{
	struct dircache *dc;

	if(m->e.mask & IN_IGNORED) {
		/* Ignore IN_IGNORED events - we'll already have removed the
		 * wd from the dircache in the callback function.
		 */
		return 0;
	}

	dc = dircache_lookup_wd(&droot, m->e.wd);
	if(!dc) {
		fprintf(stderr, "Error: dircache entry not found for wd %i\n",
			m->e.wd);
		return -1;
	}

	if(m->e.mask & IN_MOVED_TO && m->from_event) {
		struct moved_from_event *mfe = m->from_event;
		struct dircache *from_dc;

		from_dc = dircache_lookup_wd(&droot, mfe->m->e.wd);
		if(!from_dc) {
			fprintf(stderr, "Error: dircache entry not found for from event wd %i\n", mfe->m->e.wd);
			return -1;
		}
		if(m->e.mask & IN_ISDIR) {
			struct tup_entry *tent;
			int fd;
			int rc;

			if(tup_db_select_tent(from_dc->dt_node.tupid, mfe->m->e.name, &tent) < 0)
				return -1;
			if(!tent)
				return -1;
			if(tup_db_change_node(tent->tnode.tupid, m->e.name, dc->dt_node.tupid) < 0)
				return -1;
			if(tup_db_modify_dir(tent->tnode.tupid) < 0)
				return -1;
			fd = tup_db_open_tupid(dc->dt_node.tupid);
			if(fd < 0)
				return -1;
			/* Existing watches will be replaced by the
			 * inotify_add_watch calls that happen here. No sense
			 * in removing them all and re-creating them. The
			 * dircache already handles this case.
			 */
			rc = watch_path(dc->dt_node.tupid, fd, m->e.name, NULL, wp_callback);
			close(fd);
			if(rc < 0) {
				return -1;
			}
		} else {
			if(tup_file_mod(dc->dt_node.tupid, m->e.name) < 0)
				return -1;
			if(tup_file_del(from_dc->dt_node.tupid, mfe->m->e.name, -1) < 0)
				return -1;
		}
		free(mfe);
		return 0;
	}

	/* Handle IN_MOVED_TO events without corresponding from events as if
	 * they are IN_CREATE.
	 */
	if(m->e.mask & IN_CREATE || m->e.mask & IN_MOVED_TO) {
		int fd;
		int rc;

		fd = tup_db_open_tupid(dc->dt_node.tupid);
		if(fd < 0)
			return -1;
		rc = watch_path(dc->dt_node.tupid, fd, m->e.name, NULL, wp_callback);
		close(fd);
		return rc;
	}
	if(!(m->e.mask & IN_ISDIR) &&
	   (m->e.mask & IN_MODIFY || m->e.mask & IN_ATTRIB)) {
		if(tup_file_mod(dc->dt_node.tupid, m->e.name) < 0)
			return -1;
	}
	if(m->e.mask & IN_DELETE) {
		if(tup_file_del(dc->dt_node.tupid, m->e.name, -1) < 0)
			return -1;
	}
	if(m->e.mask & IN_MOVED_FROM) {
		/* IN_MOVED_FROM only happens here if the event is never
		 * claimed by a corresponding IN_MOVED_TO event. For example,
		 * if a directory tree or file is moved outside of tup's
		 * jurisdiction, or if the event is simply unloved and dies
		 * alone.
		 */
		if(tup_file_del(dc->dt_node.tupid, m->e.name, -1) < 0)
			return -1;

		/* An IN_MOVED_FROM event points to itself */
		list_del(&m->from_event->list);
	}
	return 0;
}

static void pinotify(void)
{
	perror("inotify_add_watch");
	if(errno == ENOSPC) {
		fprintf(stderr, "tup: try to increase /proc/sys/fs/inotify/max_user_watches ?\n");
	}
}

static int dump_dircache(void)
{
	struct rb_node *rbn;
	int rc = 0;

	printf("Dircache:\n");
	for(rbn = rb_first(&droot.wd_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tt;
		struct dircache *dc;
		struct tup_entry *tent;

		tt = rb_entry(rbn, struct tupid_tree, rbn);
		dc = container_of(tt, struct dircache, wd_node);

		tent = tup_entry_find(dc->dt_node.tupid);
		if(!tent) {
			printf("  [31mwd %lli: [%lli] not found[0m\n", dc->wd_node.tupid, dc->dt_node.tupid);
			rc = -1;
		} else {
			printf("  wd %lli: [%lli] ", dc->wd_node.tupid, dc->dt_node.tupid);
			print_tup_entry(tent);
			printf("\n");
		}
	}
	return rc;
}

static void sighandler(int sig)
{
	if(sig == SIGUSR1) {
		if(dump_dircache() < 0) {
			monitor_set_pid(-1);
			exit(-1);
		}
	} else {
		monitor_set_pid(-1);
		/* TODO: gracefully close, or something? */
		exit(0);
	}
}
