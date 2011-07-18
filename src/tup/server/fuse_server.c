/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "tup/server.h"
#include "tup/entry.h"
#include "tup/config.h"
#include "tup/lock.h"
#include "tup/flist.h"
#include "tup/debug.h"
#include "tup_fuse_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mount.h>
#include <sys/wait.h>

#define TUP_MNT ".tup/mnt"

#ifdef __APPLE__
  #define umount2(path, flags) unmount(path, flags)
#endif

static struct fuse_server {
	pthread_t pid;
	struct fuse *fuse;
	struct fuse_chan *ch;
	int failed;
} fs;

static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};
static volatile sig_atomic_t sig_quit = 0;
static int server_inited = 0;

static void *fuse_thread(void *arg)
{
	if(arg) {}

	if(fuse_loop(fs.fuse) < 0) {
		perror("fuse_loop");
		fs.failed = 1;
	}
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		fs.failed = 1;
		return NULL;
	}
	fuse_unmount(TUP_MNT, fs.ch);
	fuse_destroy(fs.fuse);
	fs.fuse = NULL;
	return NULL;
}

int server_init(void)
{
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	struct flist f = {0, 0, 0};

	if(server_inited)
		return 0;

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
	sigaction(SIGUSR2, &sigact, NULL);

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	if(mkdir(TUP_MNT, 0777) < 0) {
		if(errno == EEXIST) {
			/* This directory might be still mounted, let's try to umount it */
			if(umount2(TUP_MNT, MNT_FORCE) < 0) {
				if (errno == EPERM || errno == EBUSY) {
					fprintf(stderr, "tup error: Directory " TUP_MNT " is still mounted. Please unmount it with 'sudo umount -l " TUP_MNT "'.\n");
					return -1;
				}
			}
		} else {
			perror(TUP_MNT);
			fprintf(stderr, "tup error: Unable to create FUSE mountpoint.\n");
			return -1;
		}
	} else {
#ifdef __APPLE__
		/* MacOSX is a wayward beast. On a filesystem mount event it notifies several processes such as
		 * antivirus scanner, file content indexer.
		 * The content indexer (Spotlight) creates a root-owned directory that keeps index data (.tup/mnt/.Spotlight-V100).
		 * To prevent it we use "nobrowse" fuse4x flag - it tells Spotlight to skip the fs.
		 * Unfortunately it does not help in case of a short-living filesystems (e.g. in tup tests).
		 * Tup mounts and then quickly unmounts the fs and when Sportlight checks "nobrowse" flag on a filesystem
		 * using statfs() - it gets data of the local filesystem (fuse fs is already unmount at this time).
		 * The local fs does not have "nobrowse" flag thus Spotlight creates the index directory.
		 * The only way to prevent it is to create an empty file ".metadata_never_index" in the mount folder.
		 */
		int neverindex_fd = open(TUP_MNT "/.metadata_never_index", O_WRONLY|O_CREAT, 0644);
		if (neverindex_fd >= 0) {
			close(neverindex_fd);
		} else {
			perror("create(metadata_never_index):");
		}
#endif
	}

	if(mkdir(TUP_TMP, 0777) < 0) {
		if(errno != EEXIST) {
			perror(TUP_TMP);
			fprintf(stderr, "tup error: Unable to create temporary working directory.\n");
			return -1;
		}
	}

	/* Go into the tmp directory and remove any files that may have been
	 * left over from a previous tup invocation.
	 */
	if(chdir(TUP_TMP) < 0) {
		perror(TUP_TMP);
		fprintf(stderr, "tup error: Unable to chdir to the tmp directory.\n");
		return -1;
	}
	flist_foreach(&f, ".") {
		if(f.filename[0] != '.') {
			unlink(f.filename);
		}
	}
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	/* Need a garbage arg first to count as the process name */
	if(fuse_opt_add_arg(&args, "tup") < 0)
		return -1;
	if(server_debug_enabled()) {
		if(fuse_opt_add_arg(&args, "-d") < 0)
			return -1;
	}
#ifdef __APPLE__
	if(fuse_opt_add_arg(&args, "-onobrowse,noappledouble,noapplexattr,quiet") < 0)
		return -1;
#endif

	fs.ch = fuse_mount(TUP_MNT, &args);
	if(!fs.ch) {
		perror("fuse_mount");
		goto err_out;
	}
	fs.fuse = fuse_new(fs.ch, &args, &tup_fs_oper, sizeof(tup_fs_oper), NULL);
	fuse_opt_free_args(&args);
	if(!fs.fuse) {
		perror("fuse_new");
		goto err_unmount;
	}

	if(pthread_create(&fs.pid, NULL, fuse_thread, NULL) != 0) {
		perror("pthread_create");
		goto err_unmount;
	}
	server_inited = 1;
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
	if(!server_inited)
		return 0;
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
		char virtdir[100];
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
		snprintf(virtdir, sizeof(virtdir), "@tupjob-%i", s->id);
		virtdir[sizeof(virtdir)-1] = 0;
		if(chdir(virtdir) < 0) {
			perror(virtdir);
			fprintf(stderr, "tup error: Unable to chdir to virtual job directory.\n");
			return -1;
		}
		s->my_root_fd = open(".", O_RDONLY);
		if(s->my_root_fd < 0) {
			perror(".");
			fprintf(stderr, "tup error: Unable to open the current virtual directory.\n");
			return -1;
		}
		tup_entry_set_root(s->my_root_fd, &s->old_root_fd);
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

static int virt_tup_unchdir(struct server *s)
{
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}
	tup_entry_clear_root(s->old_root_fd);
	close(s->my_root_fd);
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
	pid_t pid;
	int status;

	if(dfd) {/* TODO */}

	if(tup_fuse_add_group(s->id, &s->finfo) < 0) {
		return -1;
	}

	pid = fork();
	if(pid < 0) {
		perror("fork");
		goto err_rm_group;
	}
	if(pid == 0) {
		tup_lock_close();

		if(virt_tup_chdir(dtent, s) < 0) {
			exit(1);
		}
		server_setenv(vardict_fd);
		/* Close down stdin - it can't reliably be used during the
		 * build (for example, when building in parallel, multiple
		 * programs would have to fight over who gets it, which is just
		 * nonsensical).
		 */
		close(STDIN_FILENO);

		execl("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL);
		perror("execl");
		exit(1);
	}
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		goto err_rm_group;
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

err_rm_group:
	tup_fuse_rm_group(&s->finfo);
	return -1;
}

int server_is_dead(void)
{
	return sig_quit;
}

int server_parser_start(struct tup_entry *tent, struct server *s)
{
	if(tup_fuse_add_group(s->id, &s->finfo) < 0)
		return -1;
	if(virt_tup_chdir(tent, s) < 0) {
		tup_fuse_rm_group(&s->finfo);
		return -1;
	}
	return 0;
}

int server_parser_stop(struct server *s)
{
	int rc = 0;
	if(virt_tup_unchdir(s) < 0)
		rc = -1;
	if(tup_fuse_rm_group(&s->finfo) < 0)
		rc = -1;
	return rc;
}

static void sighandler(int sig)
{
	if(sig_quit == 0) {
		fprintf(stderr, " *** tup: signal caught - waiting for jobs to finish.\n");
		sig_quit = 1;
		/* Signal the process group, in case tup was signalled
		 * directly (just a vanilla ctrl-C at the command-line doesn't
		 * need this, but a kill -INT <pid> does).
		 */
		kill(0, sig);
	}
}
