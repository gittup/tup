/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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
#include <sys/wait.h>
#include "tup/dircache.h"
#include "tup/debug.h"
#include "tup/fileio.h"
#include "tup/config.h"
#include "tup/db.h"
#include "tup/lock.h"
#include "tup/flock.h"
#include "tup/path.h"
#include "tup/entry.h"
#include "tup/fslurp.h"
#include "tup/server.h"
#include "tup/container.h"
#include "tup/option.h"
#include "tup/timespan.h"
#include "tup/variant.h"
#include "tup/init.h"

#define MONITOR_LOOP_RETRY -2

struct moved_from_event;
struct monitor_event {
	/* from_event is valid if mask is IN_MOVED_TO or IN_MOVED_FROM */
	TAILQ_ENTRY(monitor_event) list;
	struct moved_from_event *from_event;
	int mem;
	struct inotify_event e;
};
TAILQ_HEAD(monitor_event_head, monitor_event);

struct moved_from_event {
	LIST_ENTRY(moved_from_event) list;
	struct monitor_event *m;
};
LIST_HEAD(moved_from_event_head, moved_from_event);

static int monitor_set_pid(int pid);
static int monitor_loop(void);
static int wp_callback(tupid_t newdt, int dfd, const char *file);
static int events_queued(void);
static int queue_event(struct inotify_event *e);
static int flush_queue(int do_autoupdate);
static int autoupdate(const char *cmd);
static void *wait_thread(void *arg);
static int skip_event(struct inotify_event *e);
static int eventcmp(struct inotify_event *e1, struct inotify_event *e2);
static int same_event(struct inotify_event *e1, struct inotify_event *e2);
static int ephemeral_event(struct inotify_event *e);
static struct moved_from_event *add_from_event(struct monitor_event *m);
static struct moved_from_event *check_from_events(struct inotify_event *e);
static void monitor_rmdir_cb(tupid_t dt);
static int handle_event(struct monitor_event *m, int *modified);
static void pinotify(void);
static int dump_dircache(void);
static void sighandler(int sig);

static int inot_fd;
static int tup_wd;
static int obj_wd;
static struct dircache_root droot;
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};
static struct monitor_event_head event_list;
static struct monitor_event *queue_last_e = NULL;
static char **update_argv;
static int update_argc;
static int autoupdate_flag = -1;
static int autoparse_flag = -1;
static volatile sig_atomic_t dircache_debug = 0;
static volatile sig_atomic_t monitor_quit = 0;
static struct moved_from_event_head moved_from_list = LIST_HEAD_INITIALIZER(&moved_from_list);

int monitor_supported(void)
{
	return 0;
}

int monitor(int argc, char **argv)
{
	int x;
	int rc = 0;
	int foreground;

	/* Close down the fork process, since we don't need it. */
	if(server_post_exit() < 0)
		return -1;
	foreground = tup_option_get_flag("monitor.foreground");

	TAILQ_INIT(&event_list);

	/* Arguments are cleared to "-" if they are used by the monitor. These
	 * args are also passed on to the autoupdate process if that feature is
	 * enabled, but we don't want the updater getting any args that are
	 * meant for the monitor. Ultimately the options may end up at
	 * prune_graph(), which ignores args that begin with '-'.
	 */
	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			argv[x][1] = 0;
			debug_enable("monitor");
		} else if(strcmp(argv[x], "-f") == 0 ||
			  strcmp(argv[x], "--foreground") == 0) {
			argv[x][1] = 0;
			foreground = 1;
		} else if(strcmp(argv[x], "-b") == 0 ||
			  strcmp(argv[x], "--background") == 0) {
			argv[x][1] = 0;
			foreground = 0;
		} else if(strcmp(argv[x], "-a") == 0 ||
			  strcmp(argv[x], "--autoupdate") == 0) {
			argv[x][1] = 0;
			autoupdate_flag = 1;
		} else if(strcmp(argv[x], "-n") == 0 ||
			  strcmp(argv[x], "--no-autoupdate") == 0) {
			argv[x][1] = 0;
			autoupdate_flag = 0;
		} else if(strcmp(argv[x], "--autoparse") == 0) {
			argv[x][1] = 0;
			autoparse_flag = 1;
		} else if(strcmp(argv[x], "--no-autoparse") == 0) {
			argv[x][1] = 0;
			autoparse_flag = 0;
		}
	}
	update_argc = argc;
	update_argv = argv;

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);

	if(stop_monitor(TUP_MONITOR_RESTARTING) < 0) {
		fprintf(stderr, "tup error: Unable to stop the current monitor process.\n");
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

	if(foreground) {
		if(tup_unflock(tup_sh_lock()) < 0) {
			return -1;
		}
	} else {
		if(fork() > 0) {
			/* Remove our object lock, then wait for the child
			 * process to get it.
			 */
			tup_unflock(tup_obj_lock());
			if(tup_wait_flock(tup_obj_lock()) < 0)
				exit(1);
			if(tup_cleanup() < 0)
				exit(1);
			tup_valgrind_cleanup();
			exit(0);
		}

		/* Child must re-acquire the object lock, since we lost it at
		 * the fork
		 */
		if(tup_flock(tup_obj_lock()) < 0) {
			rc = -1;
			goto close_inot;
		}
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
			struct tupid_tree *tt;
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
			while((tt = RB_ROOT(&droot.wd_root)) != NULL) {
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
	if(close(inot_fd) < 0) {
		perror("close(inot_fd)");
		rc = -1;
	}
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
	if(close(fd) < 0) {
		perror("close(fd");
		return -1;
	}
	return 0;
}

int monitor_get_pid(int restarting, int *pid)
{
	struct buf b;
	int fd;

	*pid = -1;
	fd = openat(tup_top_fd(), MONITOR_PID_FILE, O_RDWR, 0666);
	if(fd < 0) {
		if(errno != ENOENT) {
			perror(MONITOR_PID_FILE);
			return -1;
		}
		/* No pid file means we don't have the monitor running, so just
		 * leave it at -1 and return success.
		 */
		return 0;
	}
	if(tup_flock(fd) < 0) {
		return -1;
	}
	if(fslurp_null(fd, &b) < 0) {
		goto out;
	}

	if(b.len > 0) {
		*pid = strtol(b.s, NULL, 0);
	}
	free(b.s);
out:
	if(tup_unflock(fd) < 0) {
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd");
		return -1;
	}

	if(*pid > 0) {
		/* Just using getpriority() to see if the monitor process is
		 * alive.
		 */
		errno = 0;
		if(getpriority(PRIO_PROCESS, *pid) == -1 && errno == ESRCH) {
			printf("Monitor pid %i doesn't exist anymore.\n", *pid);
			if(restarting == TUP_MONITOR_RESTARTING) {
				/* If we are actually restarting the monitor
				 * make sure we let them know that the 'pid
				 * doesn't exist anymore' message isn't just
				 * an error message.
				 */
				printf("Restarting the monitor.\n");
			}
			monitor_set_pid(-1);
			*pid = -1;
		}
	}
	return 0;
}

static int autoupdate_enabled(void)
{
	int autoupdate_config;
	if(autoupdate_flag == 1)
		return 1;
	autoupdate_config = tup_option_get_flag("monitor.autoupdate");
	if(autoupdate_flag == -1 && autoupdate_config == 1)
		return 1;
	return 0;
}

static int autoparse_enabled(void)
{
	int autoparse_config;
	if(autoparse_flag == 1)
		return 1;
	autoparse_config = tup_option_get_flag("monitor.autoparse");
	if(autoparse_flag == -1 && autoparse_config == 1)
		return 1;
	return 0;
}

static int mod_cb(void *arg, struct tup_entry *tent, int style)
{
	if(tent) {}
	if(style) {}
	*(int*)arg = 1;
	return 0;
}

static int monitor_loop(void)
{
	int x;
	int rc;
	struct timespan ts;
	static char buf[(sizeof(struct inotify_event) + 16) * 4096];
	int locked = 1;

	timespan_start(&ts);

	if(tup_db_scan_begin() < 0)
		return -1;
	if(watch_path(0, tup_top_fd(), ".", wp_callback) < 0)
		return -1;
	if(tup_db_scan_end() < 0)
		return -1;

	/* If we are running in autoupdate mode, we should check to see if any
	 * files were modified while the monitor wasn't running. If so, we
	 * should run an update right away.
	 */
	if(autoupdate_enabled()) {
		int modified = 0;
		if(tup_db_begin() < 0)
			return -1;
		if(tup_db_select_node_by_flags(mod_cb, &modified, TUP_FLAGS_CREATE) < 0)
			return -1;
		if(tup_db_select_node_by_flags(mod_cb, &modified, TUP_FLAGS_MODIFY) < 0)
			return -1;
		if(tup_db_commit() < 0)
			return -1;
		if(modified) {
			if(autoupdate("autoupdate") < 0)
				return -1;
		}
	} else if(autoparse_enabled()) {
		int modified = 0;
		if(tup_db_begin() < 0)
			return -1;
		if(tup_db_select_node_by_flags(mod_cb, &modified, TUP_FLAGS_CREATE) < 0)
			return -1;
		if(tup_db_commit() < 0)
			return -1;
		if(modified) {
			if(autoupdate("autoparse") < 0)
				return -1;
		}
	}

	timespan_end(&ts);
	fprintf(stderr, "Initialized in %f seconds.\n", timespan_seconds(&ts));

	do {
		struct inotify_event *e;
		int offset = 0;

		x = read(inot_fd, buf, sizeof(buf));
		if(x < 0) {
			if(errno == EINTR) {
				/* SA_RESTART doesn't work for inotify fds */
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

			if(dircache_debug) {
				dircache_debug = 0;
				if(dump_dircache() < 0) {
					monitor_set_pid(-1);
					exit(1);
				}
			}
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
				/* If we 'rm -rf' a project with the monitor
				 * running, we will know when the db file is
				 * removed and can automatically quit the
				 * monitor.
				 */
				if(e->len && strcmp(e->name, "db") == 0) {
					printf("tup monitor: .tup file '%s' deleted - shutting down.\n", e->len ? e->name : "");
					return 0;
				}
			} else if(e->wd == obj_wd) {
				if((e->mask & IN_OPEN) && locked) {
					int pid;
					/* An autoupdate process will get the lock, so the
					 * monitor will end up here. We don't want to try
					 * to start another autoupdate in this case. However,
					 * if another tup process (such as 'tup flush') is
					 * executed, we do want to trigger an autoupdate if
					 * necessary.
					 */
					if(tup_db_begin() < 0)
						return -1;
					if(tup_db_config_get_int(AUTOUPDATE_PID, -1, &pid) < 0)
						return -1;
					if(tup_db_commit() < 0)
						return -1;
					rc = flush_queue(pid == -1);
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
					/* During an update, generated nodes (t7038, t7039) and
					 * ghost nodes (t7048) may be removed. The monitor
					 * needs to invalidate those entries, but it doesn't
					 * know from the updater which those will be. Instead
					 * we just clear out the cache, and we'll rebuild it
					 * from the database as necessary.
					 *
					 * Note that our dircache is still intact, and still
					 * references tupids. These just don't have corresponding
					 * entries until they are needed.
					 */
					if(tup_entry_clear() < 0)
						return -1;

					/* Reload the variants, since we may have new ones or
					 * deleted old ones during the update.
					 */
					variants_free();
					if(tup_db_begin() < 0)
						return -1;
					if(variant_load() < 0)
						return -1;
					if(tup_db_commit() < 0)
						return -1;
					locked = 1;
					DEBUGP("monitor ON\n");
				}
			} else {
				rc = queue_event(e);
				if(rc < 0)
					return rc;
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
				if(locked) {
					/* Timeout, flush queue */
					rc = flush_queue(1);
					if(rc < 0)
						return rc;
				}
			}
		}
	} while(!monitor_quit);

	monitor_set_pid(-1);
	return 0;
}

int stop_monitor(int restarting)
{
	int pid;

	if(monitor_get_pid(restarting, &pid) < 0) {
		fprintf(stderr, "tup error: Unable to get the current monitor pid in order to shut it down.\n");
		return -1;
	}
	if(pid < 0) {
		if(restarting == TUP_MONITOR_SHUTDOWN) {
			/* This case returns an error so we can tell in the
			 * test code if the monitor isn't actually running when
			 * it should be.
			 */
			printf("No monitor process to kill (pid < 0)\n");
			return -1;
		}
		return 0;
	}
	if(restarting == TUP_MONITOR_RESTARTING)
		printf("Restarting the monitor.\n");
	else
		printf("Shutting down the monitor.\n");
	if(kill(pid, SIGHUP) < 0) {
		perror("kill");
		return -1;
	}

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
	return !TAILQ_EMPTY(&event_list);
}

static int total_mem = 0;
static int queue_event(struct inotify_event *e)
{
	struct moved_from_event *mfe = NULL;
	struct monitor_event *m;

	if(skip_event(e))
		return 0;
	if(queue_last_e && eventcmp(&queue_last_e->e, e) == 0)
		return 0;
	if(ephemeral_event(e) == 0)
		return 0;

	if(e->mask & IN_IGNORED) {
		struct dircache *dc;
		dc = dircache_lookup_wd(&droot, e->wd);
		if(dc) {
			/* Disable this dircache until it can be cleaned up in
			 * handle_event.  This lets us skip any other events in
			 * handle_event that use this disabled dc.
			 */
			dc->dt_node.tupid = -1;
		}
	}

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

	m = malloc(sizeof(*m) + e->len);
	if(!m) {
		perror("malloc");
		return -1;
	}
	m->mem = sizeof(*m) + e->len;
	total_mem += m->mem;

	queue_last_e = m;

	memcpy(&m->e, e, sizeof(*e));
	memcpy(m->e.name, e->name, e->len);
	m->from_event = mfe;

	if(e->mask & IN_MOVED_FROM) {
		m->from_event = add_from_event(m);
	}
	TAILQ_INSERT_TAIL(&event_list, m, list);

	return 0;
}

static int flush_queue(int do_autoupdate)
{
	static int events_handled = 0;
	struct monitor_event *m;
	int overflow = 0;

	tup_db_begin();

	DEBUGP("[36mFlush[%i]: mem=%i events_handled=%i[0m\n", do_autoupdate, total_mem, events_handled);
	TAILQ_FOREACH(m, &event_list, list) {
		struct inotify_event *e;
		int modified = 0;

		e = &m->e;
		DEBUGP("Handle[%li]: '%s' %08x\n",
		       (long)sizeof(*m) + e->len, e->len ? e->name : "", e->mask);
		if(e->wd == -1) {
			overflow = 1;
		} else {
			if(e->mask != 0) {
				if(handle_event(m, &modified) < 0) {
					tup_db_rollback();
					return -1;
				}
			}
		}
		if(modified) {
			events_handled = 1;
		}
	}

	/* Free the events separately, since some events may point to earlier
	 * events in the queue with a moved_from_event pointer.
	 */
	while(!TAILQ_EMPTY(&event_list)) {
		m = TAILQ_FIRST(&event_list);
		TAILQ_REMOVE(&event_list, m, list);
		total_mem -= m->mem;
		free(m);
	}

	queue_last_e = NULL;

	tup_db_commit();
	if(overflow) {
		fprintf(stderr, "Received overflow event - restarting monitor.\n");
		return MONITOR_LOOP_RETRY;
	}

	if(events_handled && do_autoupdate) {
		events_handled = 0;
		if(autoupdate_enabled()) {
			if(autoupdate("autoupdate") < 0)
				return -1;
		} else if(autoparse_enabled()) {
			if(autoupdate("autoparse") < 0)
				return -1;
		}
	}
	return 0;
}

static int autoupdate(const char *cmd)
{
	/* This runs in a separate process (as opposed to just calling
	 * updater() directly) so it can properly get the lock from us (the
	 * monitor) and flush the queue correctly. Otherwise files touched by
	 * the updater will be caught by us after we return to regular event
	 * processing mode, which is annoying.
	 */
	pid_t pid = fork();
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(pid == 0) {
		char **args;
		int x;

		/* Make sure we start at a valid working directory (t7046) */
		if(fchdir(tup_top_fd()) < 0) {
			perror("fchdir");
			exit(1);
		}

		args = malloc((sizeof *args) * (update_argc + 3));
		if(!args) {
			perror("malloc");
			exit(1);
		}
		args[0] = strdup("tup");
		if(!args[0]) {
			perror("strdup");
			exit(1);
		}
		args[1] = strdup(cmd);
		if(!args[1]) {
			perror("strdup");
			exit(1);
		}
		args[2] = strdup("--no-environ-check");
		if(!args[2]) {
			perror("strdup");
			exit(1);
		}
		for(x=1; x<update_argc; x++) {
			args[x+2] = strdup(update_argv[x]);
			if(!args[x+2]) {
				perror("strdup");
				exit(1);
			}
		}
		args[update_argc+2] = NULL;
		execvp("tup", args);
		perror("execvp");
		exit(1);
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
		if(tup_db_begin() < 0)
			return -1;
		if(tup_db_config_set_int(AUTOUPDATE_PID, pid) < 0)
			return -1;
		if(tup_db_commit() < 0)
			return -1;
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
	if(e->len && e->name[0] == '.') {
		if(strcmp(e->name, ".gitignore") == 0)
			return 0;
		return 1;
	}
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

	TAILQ_FOREACH(m, &event_list, list) {
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
	LIST_INSERT_HEAD(&moved_from_list, mfe, list);

	return mfe;
}

static struct moved_from_event *check_from_events(struct inotify_event *e)
{
	struct moved_from_event *mfe;
	LIST_FOREACH(mfe, &moved_from_list, list) {
		if(mfe->m->e.cookie == e->cookie) {
			mfe->m->e.mask = 0;
			LIST_REMOVE(mfe, list);
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

static int handle_event(struct monitor_event *m, int *modified)
{
	struct dircache *dc;

	if(m->e.mask & IN_IGNORED) {
		/* For variant dirs that are deleted by the updater, we just
		 * get the ignore event. These don't go through the
		 * monitor_rmdir_cb, so we need to clean up here.
		 */
		dc = dircache_lookup_wd(&droot, m->e.wd);
		if(dc) {
			inotify_rm_watch(inot_fd, m->e.wd);
			dircache_del(&droot, dc);
		}
		return 0;
	}

	dc = dircache_lookup_wd(&droot, m->e.wd);
	if(!dc) {
		fprintf(stderr, "tup error: dircache entry not found for wd %i\n",
			m->e.wd);
		return -1;
	}
	if(dc->dt_node.tupid == -1) {
		/* Skip the event if the dc is disabled. It will be cleaned up
		 * soon in the IN_IGNORED block.
		 */
		return 0;
	}

	if(m->e.mask & IN_MOVED_TO && m->from_event) {
		struct moved_from_event *mfe = m->from_event;
		struct dircache *from_dc;

		from_dc = dircache_lookup_wd(&droot, mfe->m->e.wd);
		if(!from_dc) {
			fprintf(stderr, "tup error: dircache entry not found for from event wd %i\n", mfe->m->e.wd);
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
			fd = tup_db_open_tupid(dc->dt_node.tupid);
			if(fd < 0)
				return -1;
			/* Existing watches will be replaced by the
			 * inotify_add_watch calls that happen here. No sense
			 * in removing them all and re-creating them. The
			 * dircache already handles this case.
			 */
			rc = watch_path(dc->dt_node.tupid, fd, m->e.name, wp_callback);
			if(close(fd) < 0) {
				perror("close(fd)");
				return -1;
			}
			if(rc < 0) {
				return -1;
			}
		} else {
			if(tup_file_mod(dc->dt_node.tupid, m->e.name, NULL) < 0)
				return -1;
			if(tup_file_del(from_dc->dt_node.tupid, mfe->m->e.name, -1, NULL) < 0)
				return -1;
		}
		free(mfe);
		*modified = 1;
		return 0;
	}

	/* Handle IN_MOVED_TO events without corresponding from events as if
	 * they are IN_CREATE.
	 */
	if(m->e.mask & IN_CREATE || m->e.mask & IN_MOVED_TO) {
		int fd;
		int rc;
		struct tup_entry *tent;

		fd = tup_db_open_tupid(dc->dt_node.tupid);
		if(fd < 0)
			return -1;
		rc = watch_path(dc->dt_node.tupid, fd, m->e.name, wp_callback);
		if(close(fd) < 0) {
			perror("close(fd)");
			return -1;
		}
		/* Only new files (not generated files) should set the modified flag.
		 * The first time we run a command, we will get IN_MOVED_TO events
		 * for new files, but we don't want an autoupdate to trigger in
		 * this case (t7052). Note we may not get a tent if the file was
		 * already removed.
		 */
		if(tup_db_select_tent(dc->dt_node.tupid, m->e.name, &tent) < 0)
			return -1;
		if(tent && tent->type != TUP_NODE_GENERATED)
			*modified = 1;
		return rc;
	}
	if(!(m->e.mask & IN_ISDIR) &&
	   (m->e.mask & IN_MODIFY || m->e.mask & IN_ATTRIB)) {
		if(tup_file_mod(dc->dt_node.tupid, m->e.name, modified) < 0)
			return -1;
	}
	if(m->e.mask & IN_DELETE) {
		if(tup_file_del(dc->dt_node.tupid, m->e.name, -1, modified) < 0)
			return -1;
	}
	if(m->e.mask & IN_MOVED_FROM) {
		/* IN_MOVED_FROM only happens here if the event is never
		 * claimed by a corresponding IN_MOVED_TO event. For example,
		 * if a directory tree or file is moved outside of tup's
		 * jurisdiction, or if the event is simply unloved and dies
		 * alone.
		 */
		if(tup_file_del(dc->dt_node.tupid, m->e.name, -1, modified) < 0)
			return -1;

		/* An IN_MOVED_FROM event points to itself */
		LIST_REMOVE(m->from_event, list);
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
	struct tupid_tree *tt;
	int rc = 0;

	if(tup_db_begin() < 0)
		return -1;
	printf("Dircache:\n");
	RB_FOREACH(tt, tupid_entries, &droot.wd_root) {
		struct dircache *dc;
		struct tup_entry *tent;
		int tmp;

		dc = container_of(tt, struct dircache, wd_node);

		tmp = tup_entry_add(dc->dt_node.tupid, &tent);
		if(tmp < 0 || !tent) {
			printf("  [31mwd %lli: [%lli] not found[0m\n", dc->wd_node.tupid, dc->dt_node.tupid);
			rc = -1;
		} else {
			printf("  wd %lli: [%lli] ", dc->wd_node.tupid, dc->dt_node.tupid);
			print_tup_entry(stdout, tent);
			printf("\n");
		}
	}
	if(tup_db_commit() < 0)
		return -1;
	return rc;
}

static void sighandler(int sig)
{
	if(sig == SIGUSR1) {
		dircache_debug = 1;
	} else if(sig == SIGHUP) {
		monitor_quit = 1;
	} else {
		monitor_set_pid(-1);
		/* TODO: gracefully close, or something? */
		exit(0);
	}
}
