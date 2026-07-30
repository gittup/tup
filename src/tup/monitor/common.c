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

/* Shared monitor utilities used by both the inotify and fsevents backends. */

#define _ATFILE_SOURCE
#include "tup/monitor.h"
#include "monitor_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "tup/config.h"
#include "tup/db.h"
#include "tup/debug.h"
#include "tup/flock.h"
#include "tup/fslurp.h"
#include "tup/option.h"

char **update_argv;
int update_argc;
int autoupdate_flag = -1;
int autoparse_flag = -1;
pthread_mutex_t autoupdate_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t autoupdate_cond = PTHREAD_COND_INITIALIZER;
pid_t autoupdate_pid = AUTOUPDATE_NONE;

/* Arguments are cleared to "-" if they are used by the monitor. These
 * args are also passed on to the autoupdate process if that feature is
 * enabled, but we don't want the updater getting any args that are
 * meant for the monitor. Ultimately the options may end up at
 * prune_graph(), which ignores args that begin with '-'.
 */
void monitor_parse_args(int argc, char **argv, int *foreground)
{
	int x;

	*foreground = tup_option_get_flag("monitor.foreground");

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			argv[x][1] = 0;
			debug_enable("monitor");
		} else if(strcmp(argv[x], "-f") == 0 ||
			  strcmp(argv[x], "--foreground") == 0) {
			argv[x][1] = 0;
			*foreground = 1;
		} else if(strcmp(argv[x], "-b") == 0 ||
			  strcmp(argv[x], "--background") == 0) {
			argv[x][1] = 0;
			*foreground = 0;
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
}

int monitor_set_pid(int pid)
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

int autoupdate_enabled(void)
{
	int autoupdate_config;
	if(autoupdate_flag == 1)
		return 1;
	autoupdate_config = tup_option_get_flag("monitor.autoupdate");
	if(autoupdate_flag == -1 && autoupdate_config == 1)
		return 1;
	return 0;
}

int autoparse_enabled(void)
{
	int autoparse_config;
	if(autoparse_flag == 1)
		return 1;
	autoparse_config = tup_option_get_flag("monitor.autoparse");
	if(autoparse_flag == -1 && autoparse_config == 1)
		return 1;
	return 0;
}

int autoupdate(const char *cmd)
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

		args = malloc((sizeof *args) * (update_argc + 4));
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
		for(x=0; x<update_argc; x++) {
			args[x+3] = strdup(update_argv[x]);
			if(!args[x+3]) {
				perror("strdup");
				exit(1);
			}
		}
		args[update_argc+3] = NULL;
		execvp("tup", args);
		perror("execvp");
		exit(1);
	} else {
		pthread_mutex_lock(&autoupdate_lock);
		autoupdate_pid = pid;
		pthread_cond_signal(&autoupdate_cond);
		pthread_mutex_unlock(&autoupdate_lock);
		if(tup_db_begin() < 0)
			return -1;
		if(tup_db_config_set_int(AUTOUPDATE_PID, pid) < 0)
			return -1;
		if(tup_db_commit() < 0)
			return -1;
	}
	return 0;
}

void *wait_thread(void *arg)
{
	/* Apparently setting SIGCHLD to SIG_IGN isn't particularly portable,
	 * so I use this stupid thread instead. Maybe there's a better way.
	 */

	sigset_t set;
	if(arg) {/* unused */}

	/* Ignore signals so we don't catch the monitor shutdown signal. It
	 * shouldn't really matter, but helgrind complains if the wait thread
	 * writes to monitor_quit while the main thread reads from it.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGUSR1);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
		perror("pthread_sigmask()");
		exit(1);
	}

	while(1) {
		pid_t mypid;
		pthread_mutex_lock(&autoupdate_lock);
		while(autoupdate_pid == AUTOUPDATE_NONE) {
			pthread_cond_wait(&autoupdate_cond, &autoupdate_lock);
		}
		mypid = autoupdate_pid;
		autoupdate_pid = AUTOUPDATE_NONE;
		pthread_mutex_unlock(&autoupdate_lock);

		if(mypid == AUTOUPDATE_EXIT) {
			break;
		}
		if(waitpid(mypid, NULL, 0) < 0) {
			perror("waitpid");
		}
	}
	return NULL;
}
