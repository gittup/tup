/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "tup/server.h"
#include "tup/entry.h"
#include "tup/config.h"
#include "tup/flist.h"
#include "tup/debug.h"
#include "tup/fslurp.h"
#include "tup/environ.h"
#include "tup/db.h"
#include "tup/privs.h"
#include "tup/progress.h"
#include "tup/option.h"
#include "tup/variant.h"
#include "tup/container.h"
#include "tup_fuse_fs.h"
#include "master_fork.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mount.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#include <grp.h>
#endif
#include <sys/wait.h>

#define TUP_MNT ".tup/mnt"

static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};
static volatile sig_atomic_t sig_quit = 0;
static int server_inited = 0;
static int null_fd = -1;
static struct parser_server *curps;
static int privileges_dropped = 0;
static int temporarily_dropped_privileges = 0;
static gid_t original_egid;
static uid_t original_euid;
static pthread_mutex_t curps_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t fuse_tid;

static void *fuse_thread(void *arg)
{
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	if(arg) {}

	/* Need a garbage arg first to count as the process name */
	if(fuse_opt_add_arg(&args, "tup") < 0)
		return NULL;
	if(fuse_opt_add_arg(&args, "-s") < 0)
		return NULL;
	if(fuse_opt_add_arg(&args, "-f") < 0)
		return NULL;
	if(fuse_opt_add_arg(&args, TUP_MNT) < 0)
		return NULL;
	if(server_debug_enabled()) {
		if(fuse_opt_add_arg(&args, "-d") < 0)
			return NULL;
	}
	if(tup_privileged()) {
		if(fuse_opt_add_arg(&args, "-oallow_root") < 0)
			return NULL;
	}
#ifdef __APPLE__
	if(fuse_opt_add_arg(&args, "-onobrowse,noappledouble,noapplexattr,quiet") < 0)
		return NULL;
#endif
#ifdef __FreeBSD__
	if(fuse_opt_add_arg(&args, "-ouse_ino") < 0)
		return NULL;
#endif

	fuse_main(args.argc, args.argv, &tup_fs_oper, NULL);
	fuse_opt_free_args(&args);
	return NULL;
}

#if defined(__linux__)
static int os_unmount(void)
{
	int rc;
#ifdef FUSE3
	rc = system("fusermount3 -u -z " TUP_MNT);
#else
	rc = system("fusermount -u -z " TUP_MNT);
#endif
	if(rc == -1) {
		perror("system");
	}
	return rc;
}
#elif defined(__APPLE__)
static int os_unmount(void)
{
	if(unmount(TUP_MNT, MNT_FORCE) < 0) {
		perror("unmount");
		return -1;
	}
	return 0;
}
#elif defined(__FreeBSD__)
static int os_unmount(void)
{
	int rc;
	rc = system("umount -f " TUP_MNT);
	if(rc == -1) {
		perror("system");
	}
	return rc;
}
#elif defined(__NetBSD__)
static int os_unmount(void)
{
	if(unmount(TUP_MNT, MNT_FORCE) < 0) {
		perror("unmount");
		return -1;
	}
	return 0;
}
#else
#error Unsupported platform. Please add unmounting code to fuse_server.c
#endif

static int tup_unmount(void)
{
	int rc;

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		rc = -2;
	} else {
		rc = os_unmount();
	}
	if(rc != 0) {
		fprintf(stderr, "tup error: Unable to unmount the fuse file-system on .tup/mnt (return code = %i). You may have to unmount this manually as root: umount -f .tup/mnt\n", rc);
		return -1;
	}
	return 0;
}

int server_init(enum server_mode mode)
{
	struct flist f = {0, 0, 0};

	tup_fuse_set_parser_mode(mode);

	if(server_inited)
		return 0;

	tup_fuse_fs_init();

	null_fd = open("/dev/null", O_RDONLY);
	if(null_fd < 0) {
		perror("/dev/null");
		fprintf(stderr, "tup error: Unable to open /dev/null for dup'ing stdin\n");
		return -1;
	}

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
	if(sigaction(SIGUSR2, &sigact, NULL) < 0) {
		perror("sigaction");
		return -1;
	}

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	if(mkdir(TUP_MNT, 0777) < 0) {
		if(errno == EEXIST) {
			struct stat st;
			struct stat homest;
			int try_unmount = 0;
			if(stat(TUP_MNT, &st) < 0) {
				try_unmount = 1;
			} else {
				if(fstat(tup_top_fd(), &homest) < 0) {
					perror("fstat");
					fprintf(stderr, "tup error: Unable to stat the project root directory.\n");
					return -1;
				}
				if(major(st.st_dev) != major(homest.st_dev) ||
				   minor(st.st_dev) != minor(homest.st_dev)) {
					try_unmount = 1;
				}
			}
			if(try_unmount) {
				/* This directory is still mounted, let's try
				 * to umount it
				 */
				if(tup_unmount() < 0)
					return -1;
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
			if(close(neverindex_fd) < 0) {
				perror("close(neverindex_fd)");
			}
		} else {
			perror("create(neverindex_fd)");
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
			if(unlink(f.filename) != 0) {
				perror(f.filename);
				fprintf(stderr, "tup error: Unable to clean out a file in .tup/tmp directory. Please try cleaning this directory manually.\n");
				return -1;
			}
		}
	}
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	if(pthread_create(&fuse_tid, NULL, fuse_thread, NULL) != 0) {
		perror("pthread_create");
		goto err_out;
	}
	if(tup_fs_inited() < 0)
		goto err_out;

#if defined(__FreeBSD__) || defined(__APPLE__)
	/* OSX and FreeBSD have a race condition between mounting the fuse fs
	 * and the first request.  Adding an init() hook makes this less
	 * likely, but still does not prevent the race condition. The only
	 * thing that seems to work is to poll for a special file in
	 * our FUSE fs.
	 */
	{
		int x;
		char filename[PATH_MAX];
		snprintf(filename, sizeof(filename), ".tup/mnt%s/@tup@", get_tup_top());
		filename[sizeof(filename)-1] = 0;
		for(x=0; x<5000; x++) {
			struct timespec ts = {0, 1000000};
			if(access(filename, R_OK) == 0) {
				goto out_ok;
			}
			nanosleep(&ts, NULL);
		}
		fprintf(stderr, "tup error: FUSE file-system does not appear to be mounted properly.\n");
		return -1;
out_ok:
		;
	}
	/* For some reason OSXFUSE sets this to SIG_IGN, but we need it
	 * to wait for the master_fork thread.
	 */
	signal(SIGCHLD, SIG_DFL);
#endif

	server_inited = 1;
	return 0;

err_out:
	fprintf(stderr, "tup error: Unable to mount FUSE on %s\n", TUP_MNT);
	return -1;
}

int server_quit(void)
{
	if(!server_inited)
		return 0;
	if(close(null_fd) < 0) {
		perror("close(null_fd)");
	}

	if(tup_unmount() < 0)
		return -1;
	pthread_join(fuse_tid, NULL);
	return 0;
}

static int re_openat(int fd, const char *path)
{
	int newfd;

	newfd = openat(fd, path, O_RDONLY);
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	if(newfd < 0) {
		perror(path);
		return -1;
	}
	return newfd;
}

static int virt_tup_open(struct parser_server *ps)
{
	char virtdir[100];
	int fd;

	fd = openat(tup_top_fd(), TUP_MNT, O_RDONLY);
	if(fd < 0) {
		perror(TUP_MNT);
		return -1;
	}

	snprintf(virtdir, sizeof(virtdir), TUP_JOB "%i", ps->s.id);
	virtdir[sizeof(virtdir)-1] = 0;
	fd = re_openat(fd, virtdir);
	if(fd < 0) {
		fprintf(stderr, "tup error: Unable to chdir to virtual job directory.\n");
		return -1;
	}

	/* +1: Skip past top-level '/' to do a relative chdir into our fake fs. */
	ps->root_fd = re_openat(fd, get_tup_top() + 1);
	if(ps->root_fd < 0) {
		return -1;
	}

	return 0;
}

static int virt_tup_close(struct parser_server *ps)
{
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}
	if(close(ps->root_fd) < 0) {
		perror("close(s->root_fd)");
		return -1;
	}
	return 0;
}

static void server_lock(struct server *s)
{
	if(s->error_mutex)
		pthread_mutex_lock(s->error_mutex);
}

static void server_unlock(struct server *s)
{
	if(s->error_mutex)
		pthread_mutex_unlock(s->error_mutex);
}

static int finfo_wait_open_count(struct server *s)
{
	finfo_lock(&s->finfo);
	while(s->finfo.open_count > 0) {
		struct timespec ts;
		int rc;
		ts.tv_sec = time(NULL) + 10;
		ts.tv_nsec = 0;
		rc = pthread_cond_timedwait(&s->finfo.cond, &s->finfo.lock, &ts);
		if(rc != 0) {
			if(rc == ETIMEDOUT) {
				server_lock(s);
				fprintf(stderr, "tup error: FUSE did not appear to release all file descriptors after the sub-process closed.\n");
				server_unlock(s);
			} else {
				perror("pthread_cond_timedwait");
			}
			finfo_unlock(&s->finfo);
			return -1;
		}
	}
	if(s->finfo.open_count < 0) {
		server_lock(s);
		fprintf(stderr, "tup internal error: open_count shouldn't be negative.\n");
		server_unlock(s);
		return -1;
	}
	finfo_unlock(&s->finfo);
	return 0;
}

static int exec_internal(struct server *s, const char *cmd, struct tup_env *newenv,
			 struct tup_entry *dtent, int single_output, int need_namespacing,
			 int run_in_bash)
{
	int status;
	char buf[64];
	char job[JOB_MAX];
	char dir[PATH_MAX];
	struct execmsg em;
	struct variant *variant;

	memset(&em, 0, sizeof(em));
	em.sid = s->id;
	em.single_output = single_output;
	em.need_namespacing = need_namespacing;
	em.run_in_bash = run_in_bash;
	em.envlen = newenv->block_size;
	em.num_env_entries = newenv->num_entries;
	em.joblen = snprintf(job, sizeof(job), TUP_MNT "/" TUP_JOB "%i", s->id) + 1;

	/* dirlen includes the \0, which snprintf does not count. Hence the -1/+1
	 * adjusting.
	 */
	strncpy(dir, get_tup_top() + 1, sizeof(dir) - 1);
	em.dirlen = get_tup_top_len() - 1;
	em.dirlen += snprint_tup_entry(dir + em.dirlen,
				       sizeof(dir) - em.dirlen - 1,
				       variant_tent_to_srctent(dtent)) + 1;
	if(em.joblen >= JOB_MAX || em.dirlen >= PATH_MAX) {
		server_lock(s);
		fprintf(stderr, "tup error: Directory for tup entry %lli is too long.\n", dtent->tnode.tupid);
		print_tup_entry(stderr, dtent);
		server_unlock(s);
		return -1;
	}
	em.cmdlen = strlen(cmd) + 1;
	variant = tup_entry_variant(dtent);
	em.vardictlen = variant->vardict_len;
	if(master_fork_exec(&em, job, dir, cmd, newenv->envblock, variant->vardict_file, &status) < 0) {
		server_lock(s);
		fprintf(stderr, "tup error: Unable to fork sub-process.\n");
		server_unlock(s);
		return -1;
	}

	if(finfo_wait_open_count(s) < 0)
		return -1;

	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	s->output_fd = openat(tup_top_fd(), buf, O_RDONLY);
	if(s->output_fd < 0) {
		server_lock(s);
		perror(buf);
		fprintf(stderr, "tup error: Unable to open sub-process output file.\n");
		server_unlock(s);
		return -1;
	}
	if(unlinkat(tup_top_fd(), buf, 0) < 0) {
		server_lock(s);
		perror(buf);
		fprintf(stderr, "tup error: Unable to unlink sub-process output file.\n");
		server_unlock(s);
		return -1;
	}

	if(!single_output) {
		snprintf(buf, sizeof(buf), ".tup/tmp/errors-%i", s->id);
		buf[sizeof(buf)-1] = 0;
		s->error_fd = openat(tup_top_fd(), buf, O_RDWR);
		if(s->error_fd < 0) {
			server_lock(s);
			perror(buf);
			fprintf(stderr, "tup error: Unable to open sub-process errors file.\n");
			server_unlock(s);
			return -1;
		}
		if(unlinkat(tup_top_fd(), buf, 0) < 0) {
			server_lock(s);
			perror(buf);
			fprintf(stderr, "tup error: Unable to unlink sub-process errors file.\n");
			server_unlock(s);
			return -1;
		}
	}

	if(WIFEXITED(status)) {
		s->exited = 1;
		s->exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		s->signalled = 1;
		s->exit_sig = WTERMSIG(status);
	} else {
		server_lock(s);
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		server_unlock(s);
		return -1;
	}
	return 0;
}

int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent, int need_namespacing, int run_in_bash)
{
	int rc;

	if(dfd) {/* TODO */}

	if(tup_fuse_add_group(s->id, &s->finfo) < 0)
		return -1;

	rc = exec_internal(s, cmd, newenv, dtent, 1, need_namespacing, run_in_bash);

	if(tup_fuse_rm_group(&s->finfo) < 0)
		return -1;

	return rc;
}

int server_postexec(struct server *s)
{
	if(s) {}
	return 0;
}

int server_unlink(void)
{
	/* No need to unlink errant files with the FUSE server since they are
	 * created in a temporary sandbox.
	 */
	return 0;
}

int server_run_script(FILE *f, tupid_t tupid, const char *cmdline,
		      struct tent_entries *env_root, char **rules)
{
	struct tup_entry *tent;
	struct server s;
	struct tup_env te;

	if(tup_db_get_environ(env_root, NULL, &te) < 0)
		return -1;

	s.id = tupid;
	s.output_fd = -1;
	s.error_fd = -1;
	s.exited = 0;
	s.exit_status = 0;
	s.signalled = 0;
	s.error_mutex = NULL;
	tent = tup_entry_get(tupid);
	init_file_info(&s.finfo, 0);
	if(exec_internal(&s, cmdline, &te, tent, 0, 0, 0) < 0)
		return -1;
	environ_free(&te);

	if(display_output(s.error_fd, 1, cmdline, 1, f) < 0)
		return -1;
	if(close(s.error_fd) < 0) {
		perror("close(s.error_fd)");
		return -1;
	}

	if(s.exited) {
		if(s.exit_status == 0) {
			struct buf b;
			if(fslurp_null(s.output_fd, &b) < 0)
				return -1;
			if(close(s.output_fd) < 0) {
				perror("close(s.output_fd)");
				return -1;
			}
			*rules = b.s;
			return 0;
		}
		fprintf(f, "tup error: run-script exited with failure code: %i\n", s.exit_status);
	} else {
		if(s.signalled) {
			fprintf(f, "tup error: run-script terminated with signal %i\n", s.exit_sig);
		} else {
			fprintf(f, "tup error: run-script terminated abnormally.\n");
		}
	}
	return -1;
}

int serverless_run_script(FILE *f, const char *cmdline,
		          struct tent_entries *env_root, char **rules)
{
	struct tup_env te;

	if(tup_db_get_environ(env_root, NULL, &te) < 0)
		return -1;

	int exit_status = -1;
	FILE *ofile;
	FILE *efile;

	ofile = tmpfile();
	if(!ofile) {
		perror("tmpfile");
		fprintf(stderr, "tup error: Unable to create temporary file for sub-process output.\n");
		return -1;
	}

	efile = tmpfile();
	if(!efile) {
		perror("tmpfile");
		fprintf(stderr, "tup error: Unable to create temporary file for sub-process errors.\n");
		return -1;
	}
	pid_t pid = fork();

	if(pid == -1) {
		perror("fork");
		return -1;
	} else if(pid > 0) {
		waitpid(pid, &exit_status, 0);
	} else {
		if(dup2(fileno(ofile), STDOUT_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
			exit(-1);
		}
		if(dup2(fileno(efile), STDERR_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stderr for the child process.\n");
			exit(-1);
		}
		if(fclose(ofile) < 0) {
			perror("fclose(ofile)");
			exit(-1);
		}
		if(fclose(efile) < 0) {
			perror("fclose(efile)");
			exit(-1);
		}
		int subprocess_null_fd = open("/dev/null", O_RDONLY);
		if(subprocess_null_fd < 0) {
			perror("/dev/null");
			fprintf(stderr, "tup error: Unable to open /dev/null for dup'ing stdin\n");
			exit(-1);
		}
		if(dup2(subprocess_null_fd, STDIN_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stdin for child processes.\n");
			exit(-1);
		}
		if(close(subprocess_null_fd) < 0) {
			perror("close(null_fd)");
			exit(-1);
		}

		char **envp;
		char **curp;
		char *curenv;

		envp = malloc((te.num_entries + 2) * sizeof(*envp));
		if(!envp) {
			perror("malloc");
			exit(1);
		}
		/* Convert from Windows-style environment to
		 * Linux-style.
		 */
		curp = envp;
		curenv = te.envblock;
		int i = 0;
		while(*curenv && i < te.num_entries) {
			*curp = curenv;
			curp++;
			curenv += strlen(curenv) + 1;
			i++;
		}
		*curp = NULL;
		execle("/bin/sh", "/bin/sh", "-e", "-c", cmdline, NULL, envp);
	}
	environ_free(&te);

	rewind(efile);
	rewind(ofile);

	if(display_output(fileno(efile), 1, cmdline, 1, f) < 0)
		return -1;
	if(fclose(efile) < 0) {
		perror("close(efile)");
		return -1;
	}

	int exited = 0;
	int signalled = 0;
	int exit_sig = 0;

	if(WIFEXITED(exit_status)) {
		exited = 1;
		exit_status = WEXITSTATUS(exit_status);
	} else if(WIFSIGNALED(exit_status)) {
		signalled = 1;
		exit_sig = WTERMSIG(exit_status);
	} else {
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", exit_status);
		return -1;
	}

	if(exited) {
		if(exit_status == 0) {
			struct buf b;
			if(fslurp_null(fileno(ofile), &b) < 0)
				return -1;
			if(fclose(ofile) < 0) {
				perror("fclose(ofile)");
				return -1;
			}
			*rules = b.s;
			return 0;
		}
		fprintf(f, "tup error: run-script exited with failure code: %i\n", exit_status);
	} else {
		if(signalled) {
			fprintf(f, "tup error: run-script terminated with signal %i\n", exit_sig);
		} else {
			fprintf(f, "tup error: run-script terminated abnormally.\n");
		}
	}
	return -1;
}

int server_is_dead(void)
{
	return sig_quit;
}

int server_parser_start(struct parser_server *ps)
{
	pthread_mutex_lock(&curps_lock);
	if(tup_fuse_add_group(ps->s.id, &ps->s.finfo) < 0)
		goto err_unlock;
	if(virt_tup_open(ps) < 0) {
		goto err_rm_group;
	}
	ps->oldps = curps;
	curps = ps;
	pthread_mutex_unlock(&curps_lock);
	return 0;

err_rm_group:
	tup_fuse_rm_group(&ps->s.finfo);
err_unlock:
	pthread_mutex_unlock(&curps_lock);
	return -1;
}

int server_parser_stop(struct parser_server *ps)
{
	int rc = 0;
	pthread_mutex_lock(&curps_lock);
	curps = ps->oldps;
	pthread_mutex_unlock(&curps_lock);
	if(virt_tup_close(ps) < 0)
		rc = -1;
	if(tup_fuse_rm_group(&ps->s.finfo) < 0)
		rc = -1;
	/* This is probably misplaced, but there is currently no easy way to
	 * stop the server if it detects an error (in fuse_fs.c), so it just
	 * saves a flag in the file_info structure, since that's all that
	 * fuse_fs has access to. We then check it afterward the server
	 * is shutdown.
	 */
	pthread_mutex_lock(&ps->s.finfo.lock);
	if(ps->s.finfo.server_fail) {
		fprintf(stderr, "tup error: Fuse server reported an access violation.\n");
		rc = -1;
	}
	pthread_mutex_unlock(&ps->s.finfo.lock);
	return rc;
}

int tup_fuse_server_get_dir_entries(const char *path, void *buf,
				    fuse_fill_dir_t filler)
{
	struct parser_directory *pd;
	struct string_tree *st;
	int rc = -1;

	pthread_mutex_lock(&curps_lock);
	if(!curps) {
		fprintf(stderr, "tup internal error: 'curps' is not set in fuse_server.c\n");
		goto out_err;
	}
	pthread_mutex_lock(&curps->lock);
	st = string_tree_search(&curps->directories, path, strlen(path));
	if(!st) {
		/* path+1 to skip leading '/' */
		fprintf(stderr, "tup error: Unable to readdir() on directory '%s'. Run-scripts are currently limited to readdir() only the current directory, and any preloaded directories. Try using the 'preload' keyword in the Tupfile to load the directory before running the run script.\n", path+1);
		goto out_unps;
	}
	pd = container_of(st, struct parser_directory, st);

	RB_FOREACH(st, string_entries, &pd->files) {
		if(mfiller(buf, st->s, NULL, 0))
			goto out_unps;
	}
	rc = 0;
out_unps:
	pthread_mutex_unlock(&curps->lock);
out_err:
	pthread_mutex_unlock(&curps_lock);
	return rc;
}

int tup_privileged(void)
{
	if(privileges_dropped)
		return 1;
	return geteuid() == 0;
}

int tup_drop_privs(void)
{
	if(geteuid() == 0) {
#ifdef __linux__
		/* On Linux this ensures that we don't have any lingering
		 * groups with root privileges after the setgid(). On OSX we
		 * still need some groups in order to actually do the FUSE
		 * mounts.
		 */
		setgroups(0, NULL);
#endif
		if(setgid(getgid()) != 0) {
			perror("setgid");
			return -1;
		}
		if(setuid(getuid()) != 0) {
			perror("setuid");
			return -1;
		}
		privileges_dropped = 1;
	}
	return 0;
}

int tup_temporarily_drop_privs(void)
{
	if (geteuid() == 0) {
		original_egid = getegid();
		original_euid = geteuid();
		if (setegid(getgid()) != 0) {
			perror("setegid dropping");
			return -1;
		}
		if (seteuid(getuid()) != 0) {
			perror("seteuid dropping");
			return -1;
		}
		temporarily_dropped_privileges = 1;
	}
	return 0;
}

int tup_restore_privs(void)
{
	if (temporarily_dropped_privileges) {
		if (setegid(original_egid) != 0) {
			perror("setegid restoring");
			return -1;
		}
		if (seteuid(original_euid) != 0) {
			perror("seteuid restoring");
			return -1;
		}
	}
	return 0;
}

static void sighandler(int sig)
{
	if(sig_quit == 0) {
		clear_active(stderr);
		fprintf(stderr, " *** tup: signal caught - waiting for jobs to finish.\n");
		sig_quit = 1;
		/* Signal the process group, in case tup was signalled
		 * directly (just a vanilla ctrl-C at the command-line doesn't
		 * need this, but a kill -INT <pid> does).
		 */
		kill(0, sig);
	}
}
