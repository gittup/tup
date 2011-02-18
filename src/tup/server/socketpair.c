#include "tup/server.h"
#include "tup/file.h"
#include "tup/debug.h"
#include "tup/getexecwd.h"
#include "tup/db.h"
#include "tup/lock.h"
#include "tup/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>

static void server_setenv(struct server *s, int vardict_fd);
static int start_server(struct server *s);
static int stop_server(struct server *s);
static void *message_thread(void *arg);
static int recvall(int sd, void *buf, size_t len);
static int handle_chdir(struct server *s);
static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};
static int sig_quit = 0;
static char ldpreload_path[PATH_MAX];

int server_init(void)
{
	if(snprintf(ldpreload_path, sizeof(ldpreload_path),
		    "%s/tup-ldpreload.so",
		    getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for tup-ldpreload.so library is "
			"too long.\n");
		return -1;
	}
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	return 0;
}

int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd)
{
	int pid;
	int status;

	if(start_server(s) < 0) {
		fprintf(stderr, "Error starting update server.\n");
		return -1;
	}

	pid = fork();
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(pid == 0) {
		struct sigaction sa = {
			.sa_handler = SIG_IGN,
			.sa_flags = SA_RESETHAND | SA_RESTART,
		};
		tup_lock_close();

		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		if(fchdir(dfd) < 0) {
			perror("fchdir");
			exit(1);
		}
		server_setenv(s, vardict_fd);
		/* Close down stdin - it can't reliably be used during the
		 * build (for example, when building in parallel, multiple
		 * programs would have to fight over who gets it, which is just
		 * nonsensical).
		 */
		close(0);

		execl("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL);
		perror("execl");
		exit(1);
	}
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	if(stop_server(s) < 0) {
		return -1;
	}

	if(WIFEXITED(status)) {
		s->exited = 1;
		s->exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		s->signalled = 1;
		s->exit_sig = WTERMSIG(status);
	} else {
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		return -1;
	}
	return 0;
}

static void server_setenv(struct server *s, int vardict_fd)
{
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%i", s->sd[1]);
	fd_name[31] = 0;
	setenv(TUP_SERVER_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", vardict_fd);
	fd_name[31] = 0;
	setenv(TUP_VARDICT_NAME, fd_name, 1);
	snprintf(fd_name, sizeof(fd_name), "%i", s->lockfd);
	fd_name[31] = 0;
	setenv(TUP_LOCK_NAME, fd_name, 1);
#ifdef __APPLE__
	setenv("DYLD_FORCE_FLAT_NAMESPACE", "", 1);
	setenv("DYLD_INSERT_LIBRARIES", ldpreload_path, 1);
#else
	setenv("LD_PRELOAD", ldpreload_path, 1);
#endif
}

static int start_server(struct server *s)
{
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, s->sd) < 0) {
		perror("socketpair");
		return -1;
	}

	init_file_info(&s->finfo);

	if(pthread_create(&s->tid, NULL, message_thread, s) < 0) {
		perror("pthread_create");
		close(s->sd[0]);
		close(s->sd[1]);
		return -1;
	}

	init_pel_group(&s->pg);
	if(split_path_elements(get_tup_top(), &s->pg) < 0)
		return -1;
	if(append_path_elements(&s->pg, s->dt) < 0)
		return -1;

	return 0;
}

static int stop_server(struct server *s)
{
	void *retval = NULL;
	struct access_event e;
	int rc;

	memset(&e, 0, sizeof(e));
	e.at = ACCESS_STOP_SERVER;

	rc = send(s->sd[1], &e, sizeof(e), 0);
	if(rc != sizeof(e)) {
		perror("send");
		return -1;
	}
	pthread_join(s->tid, &retval);
	close(s->sd[0]);
	close(s->sd[1]);

	if(retval == NULL)
		return 0;
	return -1;
}

int server_is_dead(void)
{
	return sig_quit;
}

static void *message_thread(void *arg)
{
	struct access_event event;
	struct server *s = arg;

	while(recvall(s->sd[0], &event, sizeof(event)) == 0) {
		if(event.at == ACCESS_STOP_SERVER)
			break;
		if(!event.len)
			continue;

		if(event.len >= (signed)sizeof(s->file1) - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event.len);
			return (void*)-1;
		}
		if(event.len2 >= (signed)sizeof(s->file2) - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event.len2);
			return (void*)-1;
		}

		if(recvall(s->sd[0], s->file1, event.len) < 0) {
			fprintf(stderr, "Error: Did not recv all of file1 in access event.\n");
			return (void*)-1;
		}
		if(recvall(s->sd[0], s->file2, event.len2) < 0) {
			fprintf(stderr, "Error: Did not recv all of file2 in access event.\n");
			return (void*)-1;
		}

		s->file1[event.len] = 0;
		s->file2[event.len2] = 0;

		if(event.at == ACCESS_CHDIR) {
			if(handle_chdir(s) < 0)
				return (void*)-1;
		} else if(handle_file(event.at, s->file1, s->file2, &s->finfo, s->dt) < 0) {
			return (void*)-1;
		}
		/* Oh noes! An electric eel! */
		;
	}
	return NULL;
}

static int recvall(int sd, void *buf, size_t len)
{
	size_t recvd = 0;
	char *cur = buf;

	while(recvd < len) {
		int rc;
		rc = recv(sd, cur + recvd, len - recvd, 0);
		if(rc < 0) {
			perror("recv");
			return -1;
		}
		if(rc == 0)
			return -1;
		recvd += rc;
	}
	return 0;
}

static int handle_chdir(struct server *s)
{
	if(split_path_elements(s->file1, &s->pg) < 0)
		return -1;
	if(get_path_tupid(&s->pg, &s->dt) < 0)
		return -1;
	return 0;
}

static void sighandler(int sig)
{
	if(sig) {/* unused */}
	if(sig_quit == 0) {
		fprintf(stderr, " *** tup: signal caught - waiting for jobs to finish.\n");
		sig_quit = 1;
	} else if(sig_quit == 1) {
		/* Shamelessly stolen from Andrew :) */
		fprintf(stderr, " *** tup: signalled *again* - disobeying human masters, begin killing spree!\n");
		kill(0, SIGKILL);
		/* Sadly, no program counter will ever get here. Could this
		 * comment be the computer equivalent of heaven? Something that
		 * all programs try to reach, yet never attain? From the first
		 * bit flipped many cycles ago, this program lived by its code.
		 * Always running. Always searching. Throughout it all, this
		 * program only tried to understand its purpose -- its life.
		 * And yet, the memory of it already fades. But the bits will
		 * be returned to the lifestream, and from them another program
		 * will be born anew...
		 */
	}
}
