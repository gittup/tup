#include "tup/server.h"
#include "tup/entry.h"
#include "tup/config.h"
#include "tup/lock.h"
#include "tup/db.h" /* TODO: for TUP_DIR */
#include "tup_fuse_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

struct fuse_server {
	pthread_t pid;
	struct fuse *fuse;
	struct fuse_chan *ch;
	char mountpoint[32];
	int failed;
};

static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
};
static int sig_quit = 0;

int server_init(void)
{
	if(pthread_key_create(&fuse_key, NULL) < 0) {
		perror("pthread_key_create");
		return -1;
	}
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	return 0;
}

static void *fuse_thread(void *arg)
{
	struct server *s = arg;
	struct fuse_server *fs = s->internal;

	if(pthread_setspecific(fuse_key, arg) != 0) {
		perror("pthread_setspecific");
		return NULL;
	}

	if(fuse_loop(fs->fuse) < 0) {
		perror("fuse_loop");
		fs->failed = 1;
	}
	fuse_unmount(fs->mountpoint, fs->ch);
	fuse_destroy(fs->fuse);
	fs->fuse = NULL;
	return NULL;
}

int server_setup(struct server *s, const char *jobdir)
{
	struct fuse_server *fs;

	init_file_info(&s->finfo);

	fs = malloc(sizeof *fs);
	if(!fs) {
		perror("malloc");
		return -1;
	}

	s->internal = fs;

	if(snprintf(fs->mountpoint, sizeof(fs->mountpoint), "%s/mnt", jobdir) >= (signed)sizeof(fs->mountpoint)) {
		fprintf(stderr, "tup internal error: mountpoint is sized incorrectly.\n");
		goto err_out;
	}
	if(mkdir(fs->mountpoint, 0777) < 0) {
		if(errno != EEXIST) {
			perror("mkdirat");
			goto err_out;
		}
	}

	fs->ch = fuse_mount(fs->mountpoint, NULL);
	if(!fs->ch) {
		perror("fuse_mount");
		goto err_out;
	}
	fs->fuse = fuse_new(fs->ch, NULL, &tup_fs_oper, sizeof(tup_fs_oper), NULL);
	if(!fs->fuse) {
		perror("fuse_new");
		goto err_unmount;
	}

	if(pthread_create(&fs->pid, NULL, fuse_thread, s) != 0) {
		perror("pthread_create");
		goto err_out;
	}
	return 0;

err_unmount:
	fuse_unmount(fs->mountpoint, fs->ch);
err_out:
	fprintf(stderr, "tup error: Unable to mount FUSE on %s\n", fs->mountpoint);
	free(fs);
	return -1;

	return 0;
}

int server_quit(struct server *s, int tupfd)
{
	struct fuse_server *fs = s->internal;
	if(fs) {
		int fd;
		fuse_exit(fs->fuse);
		fd = openat(tupfd, fs->mountpoint, O_RDONLY);
		if(fd >= 0) {
			fprintf(stderr, "tup internal error: Expected open(%s) to fail on FUSE filesystem\n", fs->mountpoint);
			return -1;
		}
		pthread_join(fs->pid, NULL);
		free(fs);
		s->internal = NULL;
	}
	return 0;
}

static int virt_tup_chdir(struct tup_entry *tent, struct server *s)
{
	if(tent->parent == NULL) {
		struct fuse_server *fs = s->internal;
		if(fchdir(tup_top_fd()) < 0) {
			perror("fchdir");
			return -1;
		}
		if(chdir(TUP_DIR) < 0) {
			perror(TUP_DIR);
			return -1;
		}
		if(chdir(fs->mountpoint) < 0) {
			perror(fs->mountpoint);
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

	if(dfd) {/* TODO */}

	/* Even though this is initialized in server_setup(), if an earlier
	 * command failed to process all the files and we are in keep-going
	 * mode this may have some leftover stuff in it. So just re-initialize
	 * it.
	 */
	init_file_info(&s->finfo);

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
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
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
