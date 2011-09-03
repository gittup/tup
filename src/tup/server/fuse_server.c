/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "tup/server.h"
#include "tup/entry.h"
#include "tup/config.h"
#include "tup/lock.h"
#include "tup/flist.h"
#include "tup/debug.h"
#include "tup_fuse_fs.h"
#include "master_fork.h"
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
static int null_fd = -1;
static tupid_t curid = -1;

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
	return NULL;
}

int server_init(enum server_mode mode, struct rb_root *delete_tree)
{
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	struct flist f = {0, 0, 0};

	tup_fuse_set_parser_mode(mode, delete_tree);

	if(server_inited)
		return 0;

	null_fd = open("/dev/null", O_RDONLY);
	if(null_fd < 0) {
		perror("/dev/null");
		fprintf(stderr, "tup error: Unable to open /dev/null for dup'ing stdin\n");
		return -1;
	}

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
	close(null_fd);
	fuse_exit(fs.fuse);
	fd = openat(tup_top_fd(), TUP_MNT, O_RDONLY);
	if(fd >= 0) {
		fprintf(stderr, "tup internal error: Expected open(%s) to fail on FUSE filesystem\n", TUP_MNT);
		return -1;
	}
	pthread_join(fs.pid, NULL);
	fs.fuse = NULL;
	memset(&fs, 0, sizeof(fs));
	return 0;
}

static int re_openat(int fd, const char *path)
{
	int newfd;

	newfd = openat(fd, path, O_RDONLY);
	close(fd);
	if(newfd < 0) {
		perror(path);
		return -1;
	}
	return newfd;
}

static int virt_tup_open(struct server *s)
{
	char virtdir[100];
	int fd;

	fd = openat(tup_top_fd(), TUP_MNT, O_RDONLY);
	if(fd < 0) {
		perror(TUP_MNT);
		return -1;
	}
	/* +1: Skip past top-level '/' to do a relative chdir into our fake fs. */
	fd = re_openat(fd, get_tup_top() + 1);
	if(fd < 0) {
		return -1;
	}
	snprintf(virtdir, sizeof(virtdir), "@tupjob-%i", s->id);
	virtdir[sizeof(virtdir)-1] = 0;
	s->root_fd = re_openat(fd, virtdir);
	if(s->root_fd < 0) {
		fprintf(stderr, "tup error: Unable to chdir to virtual job directory.\n");
		return -1;
	}

	return 0;
}

static int virt_tup_close(struct server *s)
{
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}
	close(s->root_fd);
	return 0;
}

static void server_setenv(void)
{
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%i", tup_vardict_fd());
	fd_name[31] = 0;
	setenv(TUP_VARDICT_NAME, fd_name, 1);
}

int server_exec(struct server *s, int dfd, const char *cmd,
		struct tup_entry *dtent)
{
	int status;
	struct execmsg em;

	if(dfd) {/* TODO */}

	if(tup_fuse_add_group(s->id, &s->finfo) < 0) {
		return -1;
	}

	em.sid = s->id;
	/* dirlen includes the \0, which snprintf does not count. Hence the -1/+1
	 * adjusting.
	 */
	em.dirlen = snprintf(em.text, PATH_MAX - 1, TUP_MNT "/%s/@tupjob-%i", get_tup_top()+1, s->id);
	em.dirlen += snprint_tup_entry(em.text + em.dirlen,
					PATH_MAX - em.dirlen - 1,
					dtent) + 1;
	if(em.dirlen >= PATH_MAX) {
		fprintf(stderr, "tup error: Directory for tup entry %lli is too long.\n", dtent->tnode.tupid);
		print_tup_entry(stderr, dtent);
		return -1;
	}
	em.cmdlen = strlen(cmd) + 1;
	if(em.cmdlen >= PATH_MAX) {
		fprintf(stderr, "tup error: Command string '%s' is too long.\n", cmd);
		return -1;
	}
	memcpy(em.text + em.dirlen, cmd, em.cmdlen);
	if(master_fork_exec(&em, sizeof(em) - sizeof(em.text) + em.dirlen + em.cmdlen, &status) < 0) {
		fprintf(stderr, "tup error: Unable to fork sub-process.\n");
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

static int read_full(char **dest, int fd)
{
       int size = 1024;
       int cur = 0;
       int rc;
       char *p;

       p = malloc(size);
       if(!p) {
               perror("malloc");
               return -1;
       }
       do {
               /* 1-adjusting is to save room for the \0 */
               rc = read(fd, p + cur, size - cur - 1);
               if(rc < 0) {
                       perror("read");
                       goto out_err;
               }
               if(rc == 0)
                       break;
               cur += rc;
               if(cur == size - 1) {
                       size *= 2;
                       p = realloc(p, size);
                       if(!p) {
                               perror("realloc");
                               return -1;
                       }
               }
       } while(1);

       /* Room is saved for this by the 1-adjusting in the loop */
       p[cur] = 0;

       *dest = p;
       return 0;
out_err:
       free(p);
       return -1;
}


int server_run_script(int dfd, const char *cmdline, char **rules)
{
	int pfd[2];
	pid_t pid;
	int status;

	if(pipe(pfd) < 0) {
		perror("pipe");
		return -1;
	}
	pid = fork();
	if(pid < 0) {
		perror("fork");
		goto err_pipe;
	}
	if(pid == 0) {
		if(fchdir(dfd) < 0) {
			perror("fchdir");
			exit(1);
		}

		close(pfd[0]);
		if(dup2(pfd[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
			exit(1);
		}
		if(dup2(null_fd, STDIN_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stdin for the child process.\n");
			exit(1);
		}
		server_setenv();
		execl("/bin/sh", "/bin/sh", "-e", "-c", cmdline, NULL);
		exit(1);
	}
	close(pfd[1]);
	if(read_full(rules, pfd[0]) < 0)
		goto err_kill;
	if(waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	close(pfd[0]);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0)
			return 0;
		fprintf(stderr, "tup error: run-script exited with failure code: %i\n", WEXITSTATUS(status));
	} else {
		if(WIFSIGNALED(status)) {
			fprintf(stderr, "tup error: run-script terminated with signal %i\n", WTERMSIG(status));
		} else {
			fprintf(stderr, "tup error: run-script terminated abnormally.\n");
		}
	}
	return -1;

err_kill:
	if(kill(pid, SIGKILL) < 0) {
		perror("kill");
		fprintf(stderr, "tup error: Unable to kill the run-script sub-process, pid=%i\n", pid);
	}
err_pipe:
	close(pfd[0]);
	close(pfd[1]);
	return -1;
}

int server_is_dead(void)
{
	return sig_quit;
}

int server_parser_start(struct server *s)
{
	if(tup_fuse_add_group(s->id, &s->finfo) < 0)
		return -1;
	if(virt_tup_open(s) < 0) {
		tup_fuse_rm_group(&s->finfo);
		return -1;
	}
	s->oldid = curid;
	curid = s->id;
	return 0;
}

int server_parser_stop(struct server *s)
{
	int rc = 0;
	curid = s->oldid;
	if(virt_tup_close(s) < 0)
		rc = -1;
	if(tup_fuse_rm_group(&s->finfo) < 0)
		rc = -1;
	/* This is probably misplaced, but there is currently no easy way to
	 * stop the server if it detects an error (in fuse_fs.c), so it just
	 * saves a flag in the file_info structure, since that's all that
	 * fuse_fs has access to. We then check it afterward the server
	 * is shutdown.
	 */
	if(s->finfo.server_fail) {
		fprintf(stderr, "tup error: Fuse server reported an access violation.\n");
		rc = -1;
	}
	return rc;
}

tupid_t tup_fuse_server_get_curid(void)
{
	return curid;
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
