#include "master_fork.h"
#include "tup/updater.h"
#include "tup/server.h"
#include "tup/db.h"
#include "tup/tupid_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>

struct rcmsg {
	tupid_t sid;
	int status;
};

struct child_waiter {
	pid_t pid;
	tupid_t sid;
};

struct status_tree {
	struct tupid_tree tnode;
	int status;
	int set;
	pthread_cond_t cond;
	pthread_mutex_t lock;
};

static pthread_mutex_t statuslock = PTHREAD_MUTEX_INITIALIZER;
static struct rb_root status_root = RB_ROOT;
static pid_t master_fork_pid;
static int msd[2];
static pthread_t cw_tid;

static int master_fork_loop(void);
static void *child_waiter(void *arg);
static void *child_wait_notifier(void *arg);
static int wait_for_my_sid(struct status_tree *st);
static void sighandler(int sig);

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
	close(msd[0]);
	if(pthread_create(&cw_tid, NULL, child_wait_notifier, NULL) < 0) {
		perror("pthread_create");
		return -1;
	}
	return 0;
}

int server_post_exit(void)
{
	int status;
	struct execmsg em = {-1, 0, 0};
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
	close(msd[1]);
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

int master_fork_exec(struct execmsg *em, const char *dir, const char *cmd,
		     int *status)
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
	if(write_all(dir, em->dirlen) < 0)
		goto err_out;
	if(write_all(cmd, em->cmdlen) < 0)
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
			fprintf(stderr, "tup error: Expected to read %i bytes, but the master fork socket closed after %i bytes (read_all called from line %i).\n", size, bytes_read, line);
			return -1;
		}
		bytes_read += rc;
	}
	return 0;
}

static int master_fork_loop(void)
{
	struct execmsg em;
	pthread_attr_t attr;
	int null_fd;
	int vardict_fd = -2;
	char dir[PATH_MAX];
	char *cmd;
	int cmdsize = 4096;

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
	close(null_fd);

	if(pthread_attr_init(&attr) != 0) {
		perror("pthread_attr_init");
		exit(1);
	}
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		perror("pthread_attr_setdetachstate\n");
		exit(1);
	}
	while(1) {
		int ofd, efd;
		struct child_waiter *waiter;
		char buf[64];
		pid_t pid;
		pthread_t pt;

		if(read_all(msd[0], &em, sizeof(em)) < 0)
			return -1;

		/* See if we get the shutdown message. */
		if(em.sid == -1)
			break;

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

		/* Only open vardict_fd the first time we get an event (this
		 * avoids annoying synchronization between us and tup, since
		 * we really only want to open the vardict file after it has
		 * been created by the updater.
		 */
		if(vardict_fd == -2) {
			vardict_fd = open(TUP_VARDICT_FILE, O_RDONLY);
			if(vardict_fd < 0) {
				perror(TUP_VARDICT_FILE);
				fprintf(stderr, "tup error: Unable to open the vardict file.\n");
					return -1;
			}
		}

		snprintf(buf, sizeof(buf), ".tup/tmp/output-%lli", em.sid);
		buf[sizeof(buf)-1] = 0;
		ofd = creat(buf, 0600);
		if(ofd < 0) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to create temporary file for sub-process output.\n");
			return -1;
		}

		snprintf(buf, sizeof(buf), ".tup/tmp/errors-%lli", em.sid);
		buf[sizeof(buf)-1] = 0;
		efd = creat(buf, 0600);
		if(efd < 0) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to create temporary file for sub-process errors.\n");
			return -1;
		}

		pid = fork();
		if(pid < 0) {
			perror("fork");
			exit(1);
		}
		if(pid == 0) {
			char fd_name[32];

			close(msd[0]);
			snprintf(fd_name, sizeof(fd_name), "%i", vardict_fd);
			fd_name[31] = 0;
			setenv(TUP_VARDICT_NAME, fd_name, 1);
			if(chdir(dir) < 0) {
				perror("chdir");
				fprintf(stderr, "tup error: Unable to chdir to '%s'\n", dir);
				exit(1);
			}
			if(dup2(ofd, STDOUT_FILENO) < 0) {
				perror("dup2");
				fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
				exit(1);
			}
			if(dup2(efd, STDERR_FILENO) < 0) {
				perror("dup2");
				fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
				exit(1);
			}
			close(ofd);
			close(efd);
			execl("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL);
			perror("execl");
			exit(1);
		}
		close(ofd);
		close(efd);
		waiter = malloc(sizeof *waiter);
		if(!waiter) {
			perror("malloc");
			exit(1);
		}
		waiter->pid = pid;
		waiter->sid = em.sid;
		pthread_create(&pt, &attr, child_waiter, waiter);
	}

	{
		struct rcmsg rcm = {-1, 0};
		if(write(msd[0], &rcm, sizeof(rcm)) != sizeof(rcm)) {
			perror("write");
			fprintf(stderr, "tup error: Unable to send notification to shutdown the child wait thread. This process may not shutdown cleanly.\n");
			return -1;
		}
		close(msd[0]);
	}

	if(vardict_fd != -2)
		close(vardict_fd);
	if(getenv("TUP_VALGRIND")) {
		close(0);
		close(1);
		close(2);
	}
	return 0;
}

static void *child_waiter(void *arg)
{
	struct child_waiter *waiter = arg;
	struct rcmsg rcm;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	rcm.sid = waiter->sid;
	if(waitpid(waiter->pid, &rcm.status, 0) < 0) {
		perror("waitpid");
	}
	pthread_mutex_lock(&lock);
	if(write(msd[0], &rcm, sizeof(rcm)) != sizeof(rcm)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write return status value to the socket. Subprocess %i may not exit properly.\n", waiter->pid);
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
