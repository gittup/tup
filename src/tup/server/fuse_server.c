#include "tup/server.h"
#include "tup/entry.h"
#include "tup/config.h"
#include "tup/lock.h"
#include "tup/db.h" /* TODO: for TUP_DIR */
#include "tup_fuse_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

#define TUP_MNT ".tup/mnt"

static struct fuse_server {
	pthread_t pid;
	struct fuse *fuse;
	struct fuse_chan *ch;
	int failed;
} fs;

static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
};
static int sig_quit = 0;

static void *fuse_thread(void *arg)
{
	if(arg) {}

	if(fuse_loop(fs.fuse) < 0) {
		perror("fuse_loop");
		fs.failed = 1;
	}
	fuse_unmount(TUP_MNT, fs.ch);
	fuse_destroy(fs.fuse);
	fs.fuse = NULL;
	return NULL;
}

int server_init(void)
{
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	if(mkdir(TUP_MNT, 0777) < 0) {
		if(errno != EEXIST) {
			perror(TUP_MNT);
			fprintf(stderr, "tup error: Unable to create FUSE mountpoint.\n");
			return -1;
		}
	}

	fs.ch = fuse_mount(TUP_MNT, NULL);
	if(!fs.ch) {
		perror("fuse_mount");
		goto err_out;
	}
	fs.fuse = fuse_new(fs.ch, NULL, &tup_fs_oper, sizeof(tup_fs_oper), NULL);
	if(!fs.fuse) {
		perror("fuse_new");
		goto err_unmount;
	}

	if(pthread_create(&fs.pid, NULL, fuse_thread, NULL) != 0) {
		perror("pthread_create");
		goto err_unmount;
	}
	return 0;

err_unmount:
	fuse_unmount(TUP_MNT, fs.ch);
err_out:
	fprintf(stderr, "tup error: Unable to mount FUSE on %s\n", TUP_MNT);
	return -1;
}

int server_quit(void)
{
	int fd;
	fuse_exit(fs.fuse);
	fd = openat(tup_top_fd(), TUP_MNT, O_RDONLY);
	if(fd >= 0) {
		fprintf(stderr, "tup internal error: Expected open(%s) to fail on FUSE filesystem\n", TUP_MNT);
		return -1;
	}
	pthread_join(fs.pid, NULL);
	memset(&fs, 0, sizeof(fs));
	return 0;
}

static int virt_tup_chdir(struct tup_entry *tent, struct server *s)
{
	if(tent->parent == NULL) {
		if(fchdir(tup_top_fd()) < 0) {
			perror("fchdir");
			return -1;
		}
		if(chdir(TUP_MNT) < 0) {
			perror(TUP_MNT);
			return -1;
		}
		/* +1: Skip past top-level '/' to do a relative chdir into our
		 * fake fs.
		 */
		if(chdir(get_tup_top() + 1) < 0) {
			perror(get_tup_top() + 1);
			return -1;
		}
		return 0;
	}

	if(virt_tup_chdir(tent->parent, s) < 0)
		return -1;

	if(chdir(tent->name.s) < 0) {
		perror(tent->name.s);
		return -1;
	}
	return 0;
}

static void server_setenv(int vardict_fd)
{
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%i", vardict_fd);
	fd_name[31] = 0;
	setenv(TUP_VARDICT_NAME, fd_name, 1);
}

int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd,
		struct tup_entry *dtent)
{
	int pid;
	int status;
	int fd[2];

	if(dfd) {/* TODO */}

	if(pipe(fd) < 0) {
		perror("pipe");
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
		char c;
		tup_lock_close();

		/* Initialize our process group to be our PID. This is used by
		 * our FUSE filesystem to make sure only allowed processes can
		 * access it, and properly save dependencies when multiple
		 * jobs access the filesystem in parallel.
		 */
		setpgid(0, 0);

		/* Wait until the parent process is able to store away our PID
		 * in the fuse tree so it knows who we are when we access
		 * the filesystem.
		 */
		close(fd[1]);
		if(read(fd[0], &c, 1) != 1) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read from startup pipe\n");
			exit(1);
		}
		close(fd[0]);


		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		if(virt_tup_chdir(dtent, s) < 0) {
			exit(1);
		}
		server_setenv(vardict_fd);
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
	if(tup_fuse_add_group(pid, &s->finfo) < 0) {
		return -1;
	}
	close(fd[0]);
	if(write(fd[1], "\n", 1) != 1) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write to startup pipe.\n");
		return -1;
	}
	close(fd[1]);
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	if(tup_fuse_rm_group(&s->finfo) < 0) {
		return -1;
	}
	/* TODO: Add check to count number of opens+releases to make sure we end
	 * up back at 0?
	 */

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

int server_is_dead(void)
{
	return sig_quit;
}

static void sighandler(int sig)
{
	if(sig) {}

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
