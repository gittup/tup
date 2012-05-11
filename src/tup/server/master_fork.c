/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

#include "master_fork.h"
#include "tup/server.h"
#include "tup/db_types.h"
#include "tup/tupid_tree.h"
#include "tup/container.h"
#include "tup/privs.h"
#include "tup/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <signal.h>

struct rcmsg {
	tupid_t sid;
	int status;
};

struct child_waiter {
	pid_t pid;
	tupid_t sid;
	int do_chroot;
	char dev[JOB_MAX];
};

struct status_tree {
	struct tupid_tree tnode;
	int status;
	int set;
	pthread_cond_t cond;
	pthread_mutex_t lock;
};

static pthread_mutex_t statuslock = PTHREAD_MUTEX_INITIALIZER;
static struct tupid_entries status_root = {NULL};
static pid_t master_fork_pid;
static int msd[2];
static pthread_t cw_tid;

static int master_fork_loop(void);
static void *child_waiter(void *arg);
static void *child_wait_notifier(void *arg);
static int wait_for_my_sid(struct status_tree *st);
static void sighandler(int sig);
static int inited = 0;

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};

int server_pre_init(void)
{
	if(socketpair(AF_LOCAL, SOCK_STREAM, 0, msd) < 0) {
		perror("socketpair");
		return -1;
	}
	master_fork_pid = fork();
	if(master_fork_pid < 0) {
		perror("fork");
		return -1;
	}
	if(master_fork_pid == 0) {
		close(msd[1]);
		exit(master_fork_loop());
	}
	if(close(msd[0]) < 0) {
		perror("close(msd[0])");
		return -1;
	}
	if(pthread_create(&cw_tid, NULL, child_wait_notifier, NULL) < 0) {
		perror("pthread_create");
		return -1;
	}
	inited = 1;
	return 0;
}

int server_post_exit(void)
{
	int status;
	struct execmsg em = {-1, 0, 0, 0, 0, 0, 0, 0, 0};

	if(!inited)
		return 0;

	if(write(msd[1], &em, sizeof(em)) != sizeof(em)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write to the master fork socket. This process may not shutdown properly.\n");
		return -1;
	}
	if(waitpid(master_fork_pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	if(status != 0) {
		fprintf(stderr, "tup error: Master fork process returned %i\n", status);
		return -1;
	}
	pthread_join(cw_tid, NULL);
	/* Only close our side of the socket once the child_wait_notifier has
	 * joined, since that thread needs to read from it.
	 */
	if(close(msd[1]) < 0) {
		perror("close(msd[1])");
		return -1;
	}
	inited = 0;
	return 0;
}

static int write_all(const void *data, int size)
{
	if(write(msd[1], data, size) != size) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write %i bytes to the master fork socket.\n", size);
		return -1;
	}
	return 0;
}

int master_fork_exec(struct execmsg *em, const char *job, const char *dir,
		     const char *cmd, const char *envstring,
		     const char *vardict_file, int *status)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	struct status_tree st;

	st.tnode.tupid = em->sid;
	if(pthread_cond_init(&st.cond, NULL) != 0) {
		perror("pthread_cond_init");
		return -1;
	}
	if(pthread_mutex_init(&st.lock, NULL) != 0) {
		perror("pthread_mutex_init");
		return -1;
	}
	st.status = 0;
	st.set = 0;

	pthread_mutex_lock(&statuslock);
	if(tupid_tree_insert(&status_root, &st.tnode) < 0) {
		fprintf(stderr, "tup error: Unable to add status tree entry.\n");
		pthread_mutex_unlock(&statuslock);
		return -1;
	}
	pthread_mutex_unlock(&statuslock);

	pthread_mutex_lock(&lock);
	if(write_all(em, sizeof(*em)) < 0)
		goto err_out;
	if(write_all(job, em->joblen) < 0)
		goto err_out;
	if(write_all(dir, em->dirlen) < 0)
		goto err_out;
	if(write_all(cmd, em->cmdlen) < 0)
		goto err_out;
	if(write_all(envstring, em->envlen) < 0)
		goto err_out;
	if(write_all(vardict_file, em->vardictlen) < 0)
		goto err_out;
	pthread_mutex_unlock(&lock);
	*status = wait_for_my_sid(&st);
	return 0;

err_out:
	pthread_mutex_unlock(&lock);
	return -1;
}

#define read_all(a, b, c) read_all_internal(a, b, c, __LINE__)
static int read_all_internal(int sd, void *dest, int size, int line)
{
	int rc;
	int bytes_read = 0;
	char *p = dest;

	while(bytes_read < size) {
		rc = read(sd, p + bytes_read, size - bytes_read);
		if(rc < 0) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read from the master fork socket (read_all called from line %i).\n", line);
			return -1;
		}
		if(rc == 0) {
			DEBUGP("tup error: Expected to read %i bytes, but the master fork socket closed after %i bytes (read_all called from line %i).\n", size, bytes_read, line);
			return -1;
		}
		bytes_read += rc;
	}
	return 0;
}

static int setup_subprocess(tupid_t sid, const char *job, const char *dir,
			    const char *dev, int single_output, int do_chroot)
{
	int ofd, efd;
	char buf[64];

	snprintf(buf, sizeof(buf), ".tup/tmp/output-%lli", sid);
	buf[sizeof(buf)-1] = 0;
	ofd = creat(buf, 0600);
	if(ofd < 0) {
		perror(buf);
		fprintf(stderr, "tup error: Unable to create temporary file for sub-process output.\n");
		return -1;
	}
	if(fchown(ofd, getuid(), getgid()) < 0) {
		perror("fchown");
		fprintf(stderr, "tup error: Unable to create temporary file for sub-process output.\n");
		return -1;
	}

	if(single_output) {
		efd = ofd;
	} else {
		snprintf(buf, sizeof(buf), ".tup/tmp/errors-%lli", sid);
		buf[sizeof(buf)-1] = 0;
		efd = creat(buf, 0600);
		if(efd < 0) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to create temporary file for sub-process errors.\n");
			return -1;
		}
		if(fchown(efd, getuid(), getgid()) < 0) {
			perror("fchown");
			fprintf(stderr, "tup error: Unable to create temporary file for sub-process errors.\n");
			return -1;
		}
	}
	if(dup2(ofd, STDOUT_FILENO) < 0) {
		perror("dup2");
		fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
		return -1;
	}
	if(dup2(efd, STDERR_FILENO) < 0) {
		perror("dup2");
		fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
		return -1;
	}
	if(close(ofd) < 0) {
		perror("close(ofd)");
		return -1;
	}
	if(!single_output) {
		if(close(efd) < 0) {
			perror("close(efd)");
			return -1;
		}
	}

	if(do_chroot) {
		if(!tup_privileged()) {
			fprintf(stderr, "tup internal error: Trying to run sub-process in a chroot, but tup is not privileged.\n");
			return -1;
		}
#ifdef __APPLE__
		if(mount("devfs", dev, MNT_DONTBROWSE, NULL) < 0) {
			perror("mount");
			fprintf(stderr, "tup error: Unable to mount /dev into fuse file-system.\n");
			return -1;
		}
#else
		/* The "tmpfs" argument is ignored since we use MS_BIND, but
		 * valgrind complains about it if we use NULL.
		 */
		if(mount("/dev", dev, "tmpfs", MS_BIND, NULL) < 0) {
			perror("mount");
			fprintf(stderr, "tup error: Unable to bind-mount /dev into fuse file-system.\n");
			return -1;
		}
#endif
		if(chroot(job) < 0) {
			perror("chroot");
			return -1;
		}
		if(chdir("/") < 0) {
			perror("chdir");
			fprintf(stderr, "tup error: Unable to chdir to root directory.\n");
			return -1;
		}
		if(tup_drop_privs() < 0) {
			return -1;
		}
	} else {
		if(tup_privileged()) {
			if(tup_drop_privs() < 0)
				return -1;
		}
		if(chdir(job) < 0) {
			perror("chdir");
			fprintf(stderr, "tup error: Unable to chdir to '%s'\n", job);
			return -1;
		}
	}
	if(chdir(dir) < 0) {
		perror("chdir");
		fprintf(stderr, "tup error: Unable to chdir to '%s'\n", dir);
		return -1;
	}
	return 0;
}

static int master_fork_loop(void)
{
	struct execmsg em;
	pthread_attr_t attr;
	int null_fd;
	int vardict_fd = -2;
	char job[PATH_MAX];
	char dir[PATH_MAX];
	char vardict_file[PATH_MAX];
	char *cmd;
	char *env;
	int cmdsize = 4096;
	int envsize = 4096;
	int in_valgrind = 0;

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
	sigaction(SIGUSR2, &sigact, NULL);

	cmd = malloc(cmdsize);
	if(!cmd) {
		perror("malloc");
		return -1;
	}
	env = malloc(envsize);
	if(!env) {
		perror("malloc");
		return -1;
	}

	if(getenv("TUP_VALGRIND")) {
		in_valgrind = 1;
	}
	if(clearenv() < 0) {
		perror("clearenv");
		return -1;
	}

	null_fd = open("/dev/null", O_RDONLY);
	if(null_fd < 0) {
		perror("/dev/null");
		fprintf(stderr, "tup error: Unable to open /dev/null for dup'ing stdin\n");
		return -1;
	}
	if(dup2(null_fd, STDIN_FILENO) < 0) {
		perror("dup2");
		fprintf(stderr, "tup error: Unable to dup stdin for child processes.\n");
		return -1;
	}
	if(close(null_fd) < 0) {
		perror("close(null_fd)");
		exit(1);
	}

	if(pthread_attr_init(&attr) != 0) {
		perror("pthread_attr_init");
		exit(1);
	}
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		perror("pthread_attr_setdetachstate\n");
		exit(1);
	}
	while(1) {
		struct child_waiter *waiter;
		pid_t pid;
		pthread_t pt;

		if(read_all(msd[0], &em, sizeof(em)) < 0)
			return -1;

		/* See if we get the shutdown message. */
		if(em.sid == -1)
			break;

		if(read_all(msd[0], job, em.joblen) < 0)
			return -1;
		if(read_all(msd[0], dir, em.dirlen) < 0)
			return -1;
		if(em.cmdlen > cmdsize) {
			free(cmd);
			cmdsize = em.cmdlen;
			cmd = malloc(cmdsize);
			if(!cmd) {
				perror("malloc");
				return -1;
			}
		}
		if(read_all(msd[0], cmd, em.cmdlen) < 0)
			return -1;
		if(em.envlen > envsize) {
			free(env);
			envsize = em.envlen;
			env = malloc(envsize);
			if(!env) {
				perror("malloc");
				return -1;
			}
		}
		if(read_all(msd[0], env, em.envlen) < 0)
			return -1;
		if(read_all(msd[0], vardict_file, em.vardictlen) < 0)
			return -1;
		vardict_fd = open(vardict_file, O_RDONLY);
		if(vardict_fd < 0) {
			if(errno != ENOENT) {
				perror(vardict_file);
				fprintf(stderr, "tup error: Unable to open the vardict file in master_fork\n");
				return -1;
			}
		}

		waiter = malloc(sizeof *waiter);
		if(!waiter) {
			perror("malloc");
			exit(1);
		}
		snprintf(waiter->dev, sizeof(waiter->dev), "%s/dev", job);

		pid = fork();
		if(pid < 0) {
			perror("fork");
			exit(1);
		}
		if(pid == 0) {
			char **envp;
			char **curp;
			char *curenv;
			char fd_name[64];

			if(close(msd[0]) < 0) {
				perror("close(msd[0])");
				exit(1);
			}

			snprintf(fd_name, sizeof(fd_name), TUP_VARDICT_NAME "=%i", vardict_fd);
			fd_name[63] = 0;

			/* +1 for the vardict variable, and +1 for the terminating
			 * NULL pointer.
			 */
			envp = malloc((em.num_env_entries + 2) * sizeof(*envp));
			if(!envp) {
				perror("malloc");
				exit(1);
			}
			/* Convert from Windows-style environment to
			 * Linux-style.
			 */
			curp = envp;
			curenv = env;
			while(*curenv) {
				*curp = curenv;
				curp++;
				curenv += strlen(curenv) + 1;
			}
			*curp = fd_name;
			curp++;
			*curp = NULL;

			if(setup_subprocess(em.sid, job, dir, waiter->dev, em.single_output, em.do_chroot) < 0)
				exit(1);
			execle("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL, envp);
			perror("execl");
			exit(1);
		}
		if(vardict_fd >= 0) {
			if(close(vardict_fd) < 0) {
				perror("close(vardict_fd)");
				return -1;
			}
		}
		waiter->pid = pid;
		waiter->sid = em.sid;
		waiter->do_chroot = em.do_chroot;
		pthread_create(&pt, &attr, child_waiter, waiter);
	}

	{
		struct rcmsg rcm;
		memset(&rcm, 0, sizeof(rcm));
		rcm.sid = -1;
		rcm.status = 0;
		if(write(msd[0], &rcm, sizeof(rcm)) != sizeof(rcm)) {
			perror("write");
			fprintf(stderr, "tup error: Unable to send notification to shutdown the child wait thread. This process may not shutdown cleanly.\n");
			return -1;
		}
		if(close(msd[0]) < 0) {
			perror("close(msd[0])");
			exit(1);
		}
	}

	if(in_valgrind) {
		if(close(STDIN_FILENO) < 0)
			perror("close(STDIN_FILENO)");
		if(close(STDOUT_FILENO) < 0)
			perror("close(STDOUT_FILENO)");
		if(close(STDERR_FILENO) < 0)
			perror("close(STDERR_FILENO)");
	}
	free(cmd);
	free(env);
	return 0;
}

static void *child_waiter(void *arg)
{
	struct child_waiter *waiter = arg;
	struct rcmsg rcm;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	memset(&rcm, 0, sizeof(rcm));
	rcm.sid = waiter->sid;
	if(waitpid(waiter->pid, &rcm.status, 0) < 0) {
		perror("waitpid");
	}
	if(waiter->do_chroot && tup_privileged()) {
		int rc;
#ifdef __APPLE__
		rc = unmount(waiter->dev, MNT_FORCE);
#else
		rc = umount2(waiter->dev, MNT_FORCE);
#endif
		if(rc < 0) {
			perror("umount");
			fprintf(stderr, "tup error: Unable to umount the /dev file-system in the chroot environment. Subprocess pid=%i may not exit properly.\n", waiter->pid);
		}
	}
	pthread_mutex_lock(&lock);
	if(write(msd[0], &rcm, sizeof(rcm)) != sizeof(rcm)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write return status value to the socket. Subprocess pid=%i may not exit properly.\n", waiter->pid);
	}
	pthread_mutex_unlock(&lock);
	free(waiter);
	return NULL;
}

static void *child_wait_notifier(void *arg)
{
	if(arg) {}
	while(1) {
		struct rcmsg rcm;
		struct tupid_tree *tt;
		struct status_tree *st;
		if(read_all(msd[1], &rcm, sizeof(rcm)) < 0)
			return NULL;
		if(rcm.sid == -1)
			return NULL;
		pthread_mutex_lock(&statuslock);
		tt = tupid_tree_search(&status_root, rcm.sid);
		if(!tt) {
			fprintf(stderr, "tup internal error: Unable to find status root entry for tupid %lli\n", rcm.sid);
			pthread_mutex_unlock(&statuslock);
			return NULL;
		}
		st = container_of(tt, struct status_tree, tnode);
		pthread_mutex_lock(&st->lock);
		tupid_tree_rm(&status_root, tt);
		st->status = rcm.status;
		st->set = 1;
		pthread_cond_signal(&st->cond);
		pthread_mutex_unlock(&st->lock);
		pthread_mutex_unlock(&statuslock);
	}
	return NULL;
}

static int wait_for_my_sid(struct status_tree *st)
{
	int status;
	pthread_mutex_lock(&st->lock);
	while(st->set == 0) {
		pthread_cond_wait(&st->cond, &st->lock);
	}
	status = st->status;
	pthread_mutex_unlock(&st->lock);
	return status;
}

static void sighandler(int sig)
{
	/* The master fork process ignores signals. If the main tup process
	 * catches a signal, it will send that signal to everything in the
	 * process group, including us. That means we won't be around to
	 * get all the waitpid() statuses from the subprocesses. Instead we
	 * just ignore signals, so we can send all the waitpid return values
	 * back to the main tup process, and then tup will send us the message
	 * to shutdown properly.
	 */
	if(sig) {}
}
