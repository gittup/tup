#include "master_fork.h"
#include "tup/updater.h"
#include "tup/server.h"
#include "tup/db.h"
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
	int sid;
	int status;
};

struct child_waiter {
	pid_t pid;
	int sid;
};

struct {
	int status;
	int set;
} status_table[MAX_JOBS];

static pthread_mutex_t statuslock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t statuscond = PTHREAD_COND_INITIALIZER;
static pid_t master_fork_pid;
static int msd[2];
static pthread_t cw_tid;

static int master_fork_loop(void);
static void *child_waiter(void *arg);
static void *child_wait_notifier(void *arg);
static int wait_for_my_sid(int sid);
static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};

int server_pre_init(void)
{
	if(socketpair(AF_LOCAL, SOCK_DGRAM, 0, msd) < 0) {
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
	int sid = -1;
	if(write(msd[1], &sid, sizeof(sid)) != sizeof(sid)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write to the master fork socket. This process may not shutdown properly.\n");
		return -1;
	}
	if(waitpid(master_fork_pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	close(msd[1]);
	if(status != 0) {
		fprintf(stderr, "tup error: Master fork process returned %i\n", status);
		return -1;
	}
	pthread_join(cw_tid, NULL);
	return 0;
}

int master_fork_exec(struct execmsg *em, int size, int *status)
{
	if(write(msd[1], em, size) != size) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write %i bytes to the master fork socket.\n", size);
		return -1;
	}
	*status = wait_for_my_sid(em->sid);
	return 0;
}

static int rc_check(int actual, int expected, int exactly)
{
	if(actual < expected) {
		fprintf(stderr, "tup error: Master fork only received %i bytes, need %s %i.\n", actual, exactly ? "exactly" : "at least", expected);
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

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
	sigaction(SIGUSR2, &sigact, NULL);

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
		int rc;
		struct child_waiter *waiter;
		pid_t pid;
		pthread_t pt;

		rc = read(msd[0], &em, sizeof(em));
		if(rc == 0)
			break;
		if(rc < 0) {
			perror("read");
			return -1;
		}
		/* See if we get the shutdown message. */
		if(rc_check(rc, sizeof(em.sid), 0) < 0)
			return -1;
		if(em.sid == -1)
			break;

		/* Make sure we have at least a header. */
		if(rc_check(rc, sizeof(em) - sizeof(em.text), 0) < 0)
			return -1;
		if(rc_check(rc, sizeof(em) - sizeof(em.text) + em.dirlen + em.cmdlen, 1) < 0)
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

		pid = fork();
		if(pid < 0) {
			perror("fork");
			exit(1);
		}
		if(pid == 0) {
			char *dir = em.text;
			char *cmd = &em.text[em.dirlen];
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
			execl("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL);
			perror("execl");
			exit(1);
		}
		waiter = malloc(sizeof *waiter);
		if(!waiter) {
			perror("malloc");
			exit(1);
		}
		waiter->pid = pid;
		waiter->sid = em.sid;
		pthread_create(&pt, &attr, child_waiter, waiter);
	}
	/* Just write something smaller than an rcmsg to stop the
	 * child_wait_notifier thread.
	 */
	if(write(msd[0], &null_fd, sizeof(null_fd)) != sizeof(null_fd)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to send notification to shutdown the child wait thread. This process may not shutdown cleanly.\n");
		return -1;
	}
	close(msd[0]);
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

	rcm.sid = waiter->sid;
	if(waitpid(waiter->pid, &rcm.status, 0) < 0) {
		perror("waitpid");
	}
	if(write(msd[0], &rcm, sizeof(rcm)) != sizeof(rcm)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write return status value to the socket. Subprocess %i may not exit properly.\n", waiter->pid);
	}
	free(waiter);
	return NULL;
}

static void *child_wait_notifier(void *arg)
{
	if(arg) {}
	while(1) {
		struct rcmsg rcm;
		if(read(msd[1], &rcm, sizeof(rcm)) != sizeof(rcm))
			return NULL;
		pthread_mutex_lock(&statuslock);
		status_table[rcm.sid].status = rcm.status;
		status_table[rcm.sid].set = 1;
		pthread_cond_broadcast(&statuscond);
		pthread_mutex_unlock(&statuslock);
	}
	return NULL;
}

static int wait_for_my_sid(int sid)
{
	int status;
	pthread_mutex_lock(&statuslock);
	while(status_table[sid].set == 0) {
		pthread_cond_wait(&statuscond, &statuslock);
	}
	status = status_table[sid].status;
	status_table[sid].set = 0;
	pthread_mutex_unlock(&statuslock);
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
