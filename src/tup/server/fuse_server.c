/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011  Mike Shal <marfey@gmail.com>
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
#include "tup_fuse_fs.h"
#include "master_fork.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mount.h>
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
	.sa_flags = SA_RESTART,
};
static volatile sig_atomic_t sig_quit = 0;
static int server_inited = 0;
static int null_fd = -1;
static struct parser_server *curps;

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

#if defined(__linux__)
static int os_unmount(void)
{
	int rc;
	rc = system("fusermount -u -z " TUP_MNT);
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
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	struct flist f = {0, 0, 0};

	tup_fuse_set_parser_mode(mode);

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
	if(tup_privileged()) {
#ifdef __APPLE__
		/* allow_root seems to not work in OSX. The option parses
		 * correctly, but chroot() fails with 'Operation not permitted'.
		 */
		if(fuse_opt_add_arg(&args, "-oallow_other") < 0)
			return -1;
#else
		if(fuse_opt_add_arg(&args, "-oallow_root") < 0)
			return -1;
#endif
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
	if(!server_inited)
		return 0;
	if(close(null_fd) < 0) {
		perror("close(null_fd)");
	}

	if(tup_unmount() < 0)
		return -1;
	pthread_join(fs.pid, NULL);
	fs.fuse = NULL;
	memset(&fs, 0, sizeof(fs));
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

static int exec_internal(struct server *s, const char *cmd, struct tup_env *newenv,
			 struct tup_entry *dtent, int single_output)
{
	int status;
	char buf[64];
	char job[PATH_MAX];
	char dir[PATH_MAX];
	struct execmsg em;

	em.sid = s->id;
	em.single_output = single_output;
	em.envlen = newenv->block_size;
	em.num_env_entries = newenv->num_entries;
	em.joblen = snprintf(job, PATH_MAX, TUP_MNT "/" TUP_JOB "%i", s->id) + 1;

	/* dirlen includes the \0, which snprintf does not count. Hence the -1/+1
	 * adjusting.
	 */
	strncpy(dir, get_tup_top() + 1, sizeof(dir));
	em.dirlen = get_tup_top_len() - 1;
	em.dirlen += snprint_tup_entry(dir + em.dirlen,
				       sizeof(dir) - em.dirlen - 1,
				       dtent) + 1;
	if(em.joblen >= PATH_MAX || em.dirlen >= PATH_MAX) {
		fprintf(stderr, "tup error: Directory for tup entry %lli is too long.\n", dtent->tnode.tupid);
		print_tup_entry(stderr, dtent);
		return -1;
	}
	em.cmdlen = strlen(cmd) + 1;
	if(master_fork_exec(&em, job, dir, cmd, newenv->envblock, &status) < 0) {
		fprintf(stderr, "tup error: Unable to fork sub-process.\n");
		return -1;
	}

	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	s->output_fd = openat(tup_top_fd(), buf, O_RDONLY);
	if(s->output_fd < 0) {
		perror(buf);
		fprintf(stderr, "tup error: Unable to open sub-process output file.\n");
		return -1;
	}
	if(unlinkat(tup_top_fd(), buf, 0) < 0) {
		perror(buf);
		fprintf(stderr, "tup error: Unable to unlink sub-process output file.\n");
		return -1;
	}

	if(!single_output) {
		snprintf(buf, sizeof(buf), ".tup/tmp/errors-%i", s->id);
		buf[sizeof(buf)-1] = 0;
		s->error_fd = openat(tup_top_fd(), buf, O_RDWR);
		if(s->error_fd < 0) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to open sub-process errors file.\n");
			return -1;
		}
		if(unlinkat(tup_top_fd(), buf, 0) < 0) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to unlink sub-process errors file.\n");
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
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		return -1;
	}
	return 0;
}

int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent)
{
	int rc;

	if(dfd) {/* TODO */}

	if(tup_fuse_add_group(s->id, &s->finfo) < 0)
		return -1;

	rc = exec_internal(s, cmd, newenv, dtent, 1);

	if(tup_fuse_rm_group(&s->finfo) < 0)
		return -1;

	return rc;
}

int server_postexec(struct server *s)
{
	if(s) {}
	return 0;
}

int server_run_script(tupid_t tupid, const char *cmdline,
		      struct tupid_entries *env_root, char **rules)
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
	tent = tup_entry_get(tupid);
	if(exec_internal(&s, cmdline, &te, tent, 0) < 0)
		return -1;
	environ_free(&te);

	if(display_output(s.error_fd, 1, cmdline, 1) < 0)
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
		fprintf(stderr, "tup error: run-script exited with failure code: %i\n", s.exit_status);
	} else {
		if(s.signalled) {
			fprintf(stderr, "tup error: run-script terminated with signal %i\n", s.exit_sig);
		} else {
			fprintf(stderr, "tup error: run-script terminated abnormally.\n");
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
	if(tup_fuse_add_group(ps->s.id, &ps->s.finfo) < 0)
		return -1;
	if(virt_tup_open(ps) < 0) {
		goto err_rm_group;
	}
	ps->oldps = curps;
	curps = ps;
	return 0;

err_rm_group:
	tup_fuse_rm_group(&ps->s.finfo);
	return -1;
}

int server_parser_stop(struct parser_server *ps)
{
	int rc = 0;
	curps = ps->oldps;
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
	if(ps->s.finfo.server_fail) {
		fprintf(stderr, "tup error: Fuse server reported an access violation.\n");
		rc = -1;
	}
	return rc;
}

int tup_fuse_server_get_dir_entries(const char *path, void *buf,
				    fuse_fill_dir_t filler)
{
	struct parser_entry *pe;
	if(!curps) {
		fprintf(stderr, "tup internal error: 'curps' is not set in fuse_server.c\n");
		return -1;
	}
	if(strcmp(path, curps->path) != 0) {
		fprintf(stderr, "tup error: Unable to readdir() on directory '%s'. Run-scripts are currently limited to readdir() only the current directory.\n", path);
		return -1;
	}
	LIST_FOREACH(pe, &curps->file_list, list) {
		if(filler(buf, pe->name, NULL, 0))
			return -1;
	}
	return 0;
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
