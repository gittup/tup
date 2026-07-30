/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2026  Mike Shal <marfey@gmail.com>
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

/* macOS FSEvents-based file monitor for tup.
 *
 * This uses FSEvents to watch the project directory tree for file changes.
 * For lock coordination with other tup processes, we poll tup_try_flock()
 * since kqueue EVFILT_VNODE cannot detect flock state changes on macOS.
 */

#define _ATFILE_SOURCE
#include "tup/monitor.h"
#include "monitor_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/event.h>
#include <errno.h>
#include <unistd.h>
#include <CoreServices/CoreServices.h>
#include <pthread.h>
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
#include "tup/option.h"
#include "tup/timespan.h"
#include "tup/variant.h"
#include "tup/init.h"
#include "tup/pel_group.h"

#define MONITOR_LOOP_RETRY -2

static int monitor_loop(void);
static int try_autoupdate(void);
static void sighandler(int sig);
static void fsevents_callback(ConstFSEventStreamRef streamRef,
                              void *clientCallBackInfo,
                              size_t numEvents,
                              void *eventPaths,
                              const FSEventStreamEventFlags eventFlags[],
                              const FSEventStreamEventId eventIds[]);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};
static volatile sig_atomic_t monitor_quit = 0;

static int locked = 1;
static int backgrounding = 0;

/* Tracks whether events occurred while locked (ie, while we have control) */
static pthread_mutex_t event_lock = PTHREAD_MUTEX_INITIALIZER;
static int events_occurred = 0;

int monitor_supported(void)
{
	return 0;
}

int monitor(int argc, char **argv)
{
	int rc = 0;
	int foreground;
	pthread_t autoupdate_thread;

	/* Close down the fork process, since we don't need it. */
	if(server_post_exit() < 0)
		return -1;
	monitor_parse_args(argc, argv, &foreground);

	if(sigemptyset(&sigact.sa_mask) < 0) {
		perror("sigemptyset");
		return -1;
	}
	if(sigaction(SIGINT, &sigact, NULL) < 0) {
		perror("sigaction");
		return -1;
	}
	if(sigaction(SIGTERM, &sigact, NULL) < 0) {
		perror("sigaction");
		return -1;
	}
	if(sigaction(SIGHUP, &sigact, NULL) < 0) {
		perror("sigaction");
		return -1;
	}
	if(sigaction(SIGUSR1, &sigact, NULL) < 0) {
		perror("sigaction");
		return -1;
	}

	if(stop_monitor(TUP_MONITOR_RESTARTING) < 0) {
		fprintf(stderr, "tup error: Unable to stop the current monitor process.\n");
		return -1;
	}

	if(foreground) {
		if(tup_unflock(tup_sh_lock()) < 0) {
			return -1;
		}
	} else {
		if(fork() > 0) {
			int kq;
			struct kevent ev;

			/* Set up kqueue to watch for the child's signal
			 * (a no-op ftruncate) on the obj-lock file. Register
			 * BEFORE unlocking so we can't miss the event.
			 */
			kq = kqueue();
			if(kq < 0) {
				perror("kqueue");
				exit(1);
			}
			EV_SET(&ev, tup_obj_lock(), EVFILT_VNODE,
			       EV_ADD | EV_ENABLE, NOTE_ATTRIB, 0, NULL);
			if(kevent(kq, &ev, 1, NULL, 0, NULL) < 0) {
				perror("kevent register");
				close(kq);
				exit(1);
			}

			/* Release obj-lock, then wait for child to signal */
			tup_unflock(tup_obj_lock());
			if(kevent(kq, NULL, 0, &ev, 1, NULL) < 0) {
				perror("kevent wait");
				close(kq);
				exit(1);
			}
			close(kq);

			if(tup_cleanup() < 0)
				exit(1);
			tup_valgrind_cleanup();
			exit(0);
		}

		/* Re-open lock files to get independent file descriptions.
		 * flock(2) locks are per-file-description and shared across
		 * fork, so we need our own fds to lock independently.
		 */
		if(tup_lock_reopen() < 0) {
			return -1;
		}
		if(tup_flock(tup_obj_lock()) < 0) {
			return -1;
		}
		backgrounding = 1;
	}

	if(monitor_set_pid(getpid()) < 0) {
		return -1;
	}

	if(pthread_create(&autoupdate_thread, NULL, wait_thread, NULL) != 0) {
		perror("pthread_create");
		return -1;
	}

	do {
		rc = monitor_loop();
		if(rc == MONITOR_LOOP_RETRY) {
			/* Need to clear out all saved structures (the dircache
			 * and tup_entries), then shut the monitor off before
			 * turning it back on. If there is a waiting 'tup'
			 * it will get the lock and update in scan mode before
			 * we return from tup_lock_init(). Then we should be
			 * good to go.
			 */
			if(tup_entry_clear() < 0)
				return -1;
			if(monitor_set_pid(-1) < 0)
				return -1;
			tup_lock_closeall();

			if(fchdir(tup_top_fd()) < 0) {
				perror("fchdir tup_top");
				return -1;
			}
			if(tup_lock_init() < 0)
				return -1;

			if(tup_unflock(tup_sh_lock()) < 0) {
				return -1;
			}
			if(monitor_set_pid(getpid()) < 0)
				return -1;
		}
	} while(rc == MONITOR_LOOP_RETRY);
	monitor_set_pid(-1);

	pthread_mutex_lock(&autoupdate_lock);
	autoupdate_pid = AUTOUPDATE_EXIT;
	pthread_cond_signal(&autoupdate_cond);
	pthread_mutex_unlock(&autoupdate_lock);
	pthread_join(autoupdate_thread, NULL);

	return rc;
}

static int mod_cb(void *arg, struct tup_entry *tent)
{
	/* Only user-visible file changes should trigger autoupdate. Skip:
	 *
	 *   GENERATED / GENERATED_DIR — autoupdate writes its own outputs,
	 *     and FSEvents delivers those writes back to us. Counting them
	 *     would fire a follow-up autoupdate on every cycle.
	 *
	 *   CMD — when a generated file changes, tup_file_mod_mtime flags
	 *     the producing command as MODIFY so the next explicit `tup
	 *     upd` re-runs it. That's a downstream propagation, not a
	 *     user edit, and we'd otherwise loop on the cmd's flag set
	 *     by our own do_scan after each autoupdate cycle.
	 *
	 * Inotify reaches the same shape via per-event filtering inside
	 * handle_event (TUP_NODE_GENERATED skips in the IN_CREATE/
	 * IN_MODIFY branches). We filter at the flag-scan level instead.
	 */
	if(tent && (tent->type == TUP_NODE_GENERATED ||
	            tent->type == TUP_NODE_GENERATED_DIR ||
	            tent->type == TUP_NODE_CMD))
		return 0;
	*(int*)arg = 1;
	return 0;
}

/* FSEvents callback - runs on the FSEvents dispatch queue thread */
static void fsevents_callback(ConstFSEventStreamRef streamRef,
                              void *clientCallBackInfo,
                              size_t numEvents,
                              void *eventPaths,
                              const FSEventStreamEventFlags eventFlags[],
                              const FSEventStreamEventId eventIds[])
{
	size_t i;
	char **paths = (char **)eventPaths;

	(void)streamRef;
	(void)clientCallBackInfo;
	(void)eventIds;

	for(i = 0; i < numEvents; i++) {
		FSEventStreamEventFlags flags = eventFlags[i];

		DEBUGP("FSEvent: '%s' flags=%08x\n", paths[i], flags);

		if(flags & (kFSEventStreamEventFlagRootChanged |
		            kFSEventStreamEventFlagMustScanSubDirs)) {
			DEBUGP("root changed or must-rescan\n");
		}

		pthread_mutex_lock(&event_lock);
		events_occurred = 1;
		pthread_mutex_unlock(&event_lock);
	}
}

static int wp_callback(tupid_t newdt, const char *file, int *skip)
{
	(void)newdt;
	(void)file;
	(void)skip;
	return 0;
}

/* Check if there are pending modifications and trigger autoupdate or
 * autoparse if configured. Returns 0 on success, -1 on error.
 *
 * Gated on AUTOUPDATE_PID being clear. Without that gate, calling this
 * twice in quick succession (e.g. once from the main loop and again
 * from the yield path before the spawned `tup autoupdate` has acquired
 * the lock and cleared the create/modify flags) fires a second
 * autoupdate against the same flags — and the second autoupdate then
 * contends for the lock too, triggering a third yield+fire, etc. The
 * AUTOUPDATE_PID config entry is set by autoupdate() and cleared by
 * the `tup autoupdate` child when it exits, so checking it tells us
 * whether a prior fire is still in flight.
 */
static int try_autoupdate(void)
{
	int modified = 0;
	int pid;
	const char *cmd;

	if(autoupdate_enabled())
		cmd = "autoupdate";
	else if(autoparse_enabled())
		cmd = "autoparse";
	else
		return 0;

	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_config_get_int(AUTOUPDATE_PID, -1, &pid) < 0)
		return -1;
	if(pid == -1) {
		if(tup_db_select_node_by_flags(mod_cb, &modified,
		                               TUP_FLAGS_CREATE) < 0)
			return -1;
		if(strcmp(cmd, "autoupdate") == 0 &&
		   tup_db_select_node_by_flags(mod_cb, &modified,
		                               TUP_FLAGS_MODIFY) < 0)
			return -1;
	}
	if(tup_db_commit() < 0)
		return -1;

	if(pid == -1 && modified && autoupdate(cmd) < 0)
		return -1;
	return 0;
}

static int do_scan(void)
{
	struct timespan ts;
	int rc;

	timespan_start(&ts);

	/* Clear variant state before re-scanning, since tup_db_scan_begin()
	 * will reload variants from the database.
	 */
	variants_free();
	if(tup_entry_clear() < 0)
		return -1;

	if(tup_db_scan_begin() < 0)
		return -1;
	/* Enable inode-based rename detection for this scan only. The
	 * one-shot scanner (`tup scan` / `tup upd` without monitor) keeps
	 * the legacy delete+create semantics — tests like t5012 rely on
	 * it. Renames inside an active monitor session, by contrast,
	 * should match what inotify's MOVED_FROM/MOVED_TO pair handling
	 * delivers: identity preserved.
	 */
	watch_path_set_detect_rename(1);
	rc = watch_path(0, ".", wp_callback);
	watch_path_set_detect_rename(0);
	if(rc < 0)
		return -1;
	if(tup_db_scan_end() < 0)
		return -1;
	timespan_end(&ts);
	DEBUGP("Scan completed in %f seconds.\n", timespan_seconds(&ts));
	return 0;
}

/* Wait for NOTE_FUNLOCK on the object lock file using kqueue. This fires
 * the instant another process releases or closes its flock on the file.
 * Returns: 0 = lock released, -1 = error
 */
static int wait_obj_unlock(void)
{
	int kq;
	struct kevent ev;
	int ret;

	kq = kqueue();
	if(kq < 0) {
		perror("kqueue");
		return -1;
	}

	EV_SET(&ev, tup_obj_lock(), EVFILT_VNODE, EV_ADD | EV_ENABLE,
	       NOTE_FUNLOCK, 0, NULL);
	if(kevent(kq, &ev, 1, NULL, 0, NULL) < 0) {
		perror("kevent register NOTE_FUNLOCK");
		close(kq);
		return -1;
	}

	/* Block until the lock is released */
	ret = kevent(kq, NULL, 0, &ev, 1, NULL);
	close(kq);
	if(ret < 0) {
		if(errno == EINTR)
			return 0;
		perror("kevent wait NOTE_FUNLOCK");
		return -1;
	}
	return 0;
}

/* Check if another tup process wants the object lock.
 *
 * The tri-lock protocol:
 * - The monitor holds the object lock and has released the shared lock.
 * - When another tup process wants to run, it first acquires the shared
 *   lock, then tries to flock the object lock (blocking).
 * - We detect this by probing the shared lock every 100ms. If we can't
 *   get it, someone else has it and is about to want the object lock.
 * - Yield: take tri-lock, release obj-lock, wait for NOTE_FUNLOCK on
 *   obj-lock (fired when the other process releases/closes it), then
 *   re-acquire obj-lock and release tri-lock.
 *
 * Returns: 0 = still online or back online, -1 = error
 */
static int check_lock_state(void)
{
	if(!locked) return 0;

	/* Probe: can we acquire the shared lock? If yes, nobody else is
	 * active, so release it and continue. If no, someone has it and
	 * we need to yield.
	 */
	int sh_rc = tup_try_flock(tup_sh_lock());
	if(sh_rc < 0) {
		return -1;
	} else if(sh_rc == 0) {
		if(tup_unflock(tup_sh_lock()) < 0) {
			return -1;
		} else {
			return 0;
		}
	}

	/* sh_rc == 1: Another process has the shared lock and is about to
	 * want the object lock. Yield using the tri-lock protocol.
	 */

	DEBUGP("shared lock contention - yielding\n");

	/* Always run do_scan before releasing the lock, regardless of
	 * events_occurred. FSEvents has ~100ms debounce/delivery latency,
	 * so a change made just before the contending process called
	 * `tup upd` may not have fired our callback yet by the time the
	 * 100ms-poll detects contention — the events_occurred flag races
	 * the lock probe. A stat-walk on yield always closes that window.
	 * Inotify reaches the same shape by draining its event queue at
	 * the yield (inotify.c handle of IN_OPEN on obj_wd → flush_queue).
	 *
	 * try_autoupdate() also runs so `tup flush` can observe the
	 * AUTOUPDATE_PID it polls for. The bomb risk (autoupdate's own
	 * writes triggering more autoupdate) is contained by mod_cb's
	 * GENERATED/CMD filter, not by suppressing autoupdate at the yield.
	 */
	pthread_mutex_lock(&event_lock);
	events_occurred = 0;
	pthread_mutex_unlock(&event_lock);
	if(do_scan() < 0)
		return -1;
	if(try_autoupdate() < 0)
		return -1;

	locked = 0;

	/* Take tri-lock (ensures we're first to get obj-lock back) */
	if(tup_flock(tup_tri_lock()) < 0)
		return -1;

	/* Release obj-lock so the other process can proceed */
	if(tup_unflock(tup_obj_lock()) < 0)
		return -1;

	DEBUGP("monitor off\n");

	/* Wait for the other process to release the object lock. kqueue
	 * NOTE_FUNLOCK fires the instant flock is released or the fd is
	 * closed - no polling, no race.
	 */
	if(wait_obj_unlock() < 0)
		return -1;

	/* Re-acquire obj-lock */
	if(tup_flock(tup_obj_lock()) < 0)
		return -1;

	/* Release tri-lock so the other process can finish tup_lock_exit()
	 * (it waits on tri-lock before releasing the shared lock).
	 */
	if(tup_unflock(tup_tri_lock()) < 0)
		return -1;

	/* During an update, generated nodes and ghost nodes may be removed.
	 * The monitor needs to invalidate those entries. We just clear out
	 * the cache and rebuild from the database as necessary.
	 */
	if(tup_entry_clear() < 0)
		return -1;

	/* Reload the variants, since we may have new ones or have deleted
	 * old ones during the update.
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

	return 0;
}

static int monitor_loop(void)
{
	int rc;
	struct timespan ts;
	FSEventStreamRef stream;
	CFStringRef path_to_watch;
	CFArrayRef paths_to_watch;
	FSEventStreamContext ctx = {0, NULL, NULL, NULL, NULL};
	dispatch_queue_t queue;

	timespan_start(&ts);

	/* Initial scan */
	if(tup_db_scan_begin() < 0)
		return -1;
	if(watch_path(0, ".", wp_callback) < 0)
		return -1;
	if(tup_db_scan_end() < 0)
		return -1;

	/* If we are running in autoupdate mode, we should check to see if
	 * any files were modified while the monitor wasn't running. If so,
	 * we should run an update right away.
	 */
	if(try_autoupdate() < 0)
		return -1;

	timespan_end(&ts);
	fprintf(stderr, "Initialized in %f seconds.\n", timespan_seconds(&ts));

	/* In background mode, signal the parent that we're ready. The
	 * parent is blocked on kqueue waiting for NOTE_ATTRIB on the
	 * obj-lock file before it exits.
	 */
	if(backgrounding) {
		if(ftruncate(tup_obj_lock(), 0) < 0) {
			perror("ftruncate obj lock");
			return -1;
		}
		backgrounding = 0;
	}

	/* Create FSEvents stream */
	path_to_watch = CFStringCreateWithCString(NULL, get_tup_top(), kCFStringEncodingUTF8);
	if(!path_to_watch) {
		fprintf(stderr, "tup error: Failed to create CFString for path\n");
		return -1;
	}
	paths_to_watch = CFArrayCreate(NULL, (const void **)&path_to_watch, 1, &kCFTypeArrayCallBacks);
	if(!paths_to_watch) {
		CFRelease(path_to_watch);
		fprintf(stderr, "tup error: Failed to create CFArray\n");
		return -1;
	}

	stream = FSEventStreamCreate(NULL,
	                             &fsevents_callback,
	                             &ctx,
	                             paths_to_watch,
	                             kFSEventStreamEventIdSinceNow,
	                             0.1, /* 100ms debounce */
	                             kFSEventStreamCreateFlagFileEvents);

	CFRelease(paths_to_watch);
	CFRelease(path_to_watch);

	if(!stream) {
		fprintf(stderr, "tup error: Failed to create FSEvent stream\n");
		return -1;
	}

	queue = dispatch_queue_create("tup.monitor.fsevents", DISPATCH_QUEUE_SERIAL);
	FSEventStreamSetDispatchQueue(stream, queue);

	if(!FSEventStreamStart(stream)) {
		fprintf(stderr, "tup error: Failed to start FSEvent stream\n");
		FSEventStreamInvalidate(stream);
		FSEventStreamRelease(stream);
		dispatch_release(queue);
		return -1;
	}

	/* Main event loop: poll for FSEvents + check lock state periodically */
	do {
		struct timeval tv;
		int have_events;
		int ret;

		/* Sleep 100ms between iterations */
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		ret = select(0, NULL, NULL, NULL, &tv);
		if(ret < 0 && errno != EINTR) {
			perror("select");
			rc = -1;
			goto stop_stream;
		}

		/* Check if .tup/db still exists */
		{
			struct stat st;
			if(stat(TUP_DIR "/db", &st) < 0 && errno == ENOENT) {
				printf("tup monitor: .tup file 'db' deleted - shutting down.\n");
				rc = 0;
				goto stop_stream;
			}
		}

		/* Check lock state (are we still the owner?) */
		if(check_lock_state() < 0) {
			rc = -1;
			goto stop_stream;
		}

		/* Process filesystem events if we're locked (have control) */
		if(locked) {
			pthread_mutex_lock(&event_lock);
			have_events = events_occurred;
			events_occurred = 0;
			pthread_mutex_unlock(&event_lock);

			if(have_events) {
				if(do_scan() < 0 || try_autoupdate() < 0) {
					rc = -1;
					goto stop_stream;
				}
			}
		}
	} while(!monitor_quit);

	rc = 0;

stop_stream:
	FSEventStreamStop(stream);
	FSEventStreamInvalidate(stream);
	FSEventStreamRelease(stream);
	dispatch_release(queue);

	monitor_set_pid(-1);
	return rc;
}

static void sighandler(int sig)
{
	if(sig == SIGUSR1) {
		/* No-op on macOS */
	} else if(sig == SIGHUP) {
		monitor_quit = 1;
	} else {
		monitor_set_pid(-1);
		/* TODO: gracefully close, or something? */
		exit(0);
	}
}
