/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2015-2020  Mike Shal <marfey@gmail.com>
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

#include "tup/server.h"
#include "tup/config.h"
#include "tup/flist.h"
#include "tup/environ.h"
#include "tup/entry.h"
#include "tup/variant.h"
#include "tup/lock.h"
#include "tup/progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define TUP_TMP ".tup/tmp"
#define LDPRELOAD_NAME "LD_PRELOAD"

static void sighandler(int sig);
static int process_depfile(struct server *s, int fd);
static int server_inited = 0;
static char ldpreload_path[PATH_MAX];

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};
static volatile sig_atomic_t sig_quit = 0;

int server_pre_init(void)
{
	int exelen;

	if(setpgid(0, 0) < 0) {
		perror("setpgid");
		fprintf(stderr, "tup error: Unable to set process group for tup's subprocesses.\n");
		return -1;
	}
	exelen = readlink("/proc/self/exe", ldpreload_path, sizeof(ldpreload_path));
	if(exelen < 0) {
		perror("/proc/self/exe");
		fprintf(stderr, "tup error: Unable to read /proc/self/exe to determine the location of the tup executable.\n");
		return -1;
	}
	ldpreload_path[exelen] = 0;
	strncpy(ldpreload_path + exelen, "-ldpreload.so", sizeof(ldpreload_path) - exelen);
	return 0;
}

int server_post_exit(void)
{
	return 0;
}

static char **server_setenv(struct tup_env *env, int vardict_fd, const char *depfile)
{
	char *preloadenv;
	char **envp;
	char **curp;
	char *curenv;
	int len;

	len = strlen(TUP_DEPFILE) + 1 + strlen(depfile) + 1;
	len += strlen(TUP_VARDICT_NAME) + 1 + 32 + 1;
	len += strlen(LDPRELOAD_NAME) + 1 + strlen(ldpreload_path) + 1;

	preloadenv = malloc(len);
	if(!preloadenv) {
		perror("malloc");
		return NULL;
	}
	snprintf(preloadenv, len, "%s=%s%c%s=%i%c%s=%s%c", TUP_DEPFILE, depfile, 0, TUP_VARDICT_NAME, vardict_fd, 0, LDPRELOAD_NAME, ldpreload_path, 0);

	/* +3 for our variables, and +1 for the terminating NULL pointer.
	 */
	envp = malloc((env->num_entries + 4) * sizeof(*envp));
	if(!envp) {
		perror("malloc");
		return NULL;
	}
	/* Convert from Windows-style environment to Linux-style.
	 */
	curp = envp;
	curenv = env->envblock;
	while(*curenv) {
		*curp = curenv;
		curp++;
		curenv += strlen(curenv) + 1;
	}
	curenv = preloadenv;
	while(*curenv) {
		*curp = curenv;
		curp++;
		curenv += strlen(curenv) + 1;
	}
	*curp = NULL;
	return envp;
}

int server_init(enum server_mode mode)
{
	struct flist f = {0, 0, 0};

	if(mode) {/* unused */}

	if(server_inited)
		return 0;

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
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
				fprintf(stderr, "tup error: Unable to clean out a file in .tup/tmp directory. Please try cleaning this directory manually. Note there may be a stuck sub-process that still has the file open (check the Task Manager).\n");
				return -1;
			}
		}
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

	server_inited = 1;
	return 0;
}

int server_quit(void)
{
	return 0;
}

static int run_subprocess(int ofd, int dfd, const char *cmd, const char *depfile, struct tup_env *env, int run_in_bash, int *status)
{
	int pid;
	int null_fd;
	int vardict_fd = -1; /* TODO */
	pid = fork();
	if(pid == 0) {
		char **envp;
		tup_lock_closeall();
		if(dup2(ofd, STDOUT_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stdout for the child process.\n");
			exit(1);
		}
		if(dup2(ofd, STDERR_FILENO) < 0) {
			perror("dup2");
			fprintf(stderr, "tup error: Unable to dup stderr for the child process.\n");
			exit(1);
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

		if(fchdir(dfd) < 0) {
			perror("fchdir");
			exit(1);
		}
		envp = server_setenv(env, vardict_fd, depfile);
		if(!envp) {
			exit(1);
		}
		if(run_in_bash) {
			execle("/usr/bin/env", "/usr/bin/env", "bash", "-e", "-o", "pipefail", "-c", cmd, NULL, envp);
		} else {
			execle("/bin/sh", "/bin/sh", "-e", "-c", cmd, NULL, envp);
		}
		perror("execl");
		exit(1);
	}
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(waitpid(pid, status, 0) < 0) {
		perror("waitpid");
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

int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent, int need_namespacing, int run_in_bash)
{
	int fd;
	char depfile[PATH_MAX];
	char buf[64];
	int status;

	if(dtent) {}
	if(need_namespacing) {}

	snprintf(depfile, PATH_MAX, "%s/%s/deps-%i", get_tup_top(), TUP_TMP, s->id);
	depfile[PATH_MAX-1] = 0;
	fd = open(depfile, O_RDONLY | O_CREAT | O_CLOEXEC, 0666);
	if(fd < 0) {
		perror(depfile);
		fprintf(stderr, "tup error: Unable to create dependency file for the sub-process.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	s->output_fd = open(buf, O_CREAT | O_RDWR | O_CLOEXEC | O_TRUNC, 0600);
	if(s->output_fd < 0) {
		perror(buf);
		return -1;
	}
	if(run_subprocess(s->output_fd, dfd, cmd, depfile, newenv, run_in_bash, &status) < 0) {
		close(fd);
		return -1;
	}
	if(process_depfile(s, fd) < 0)
		return -1;
	if(close(fd) < 0) {
		perror("close(fd)");
		fprintf(stderr, "tup error: Unable to close depfile.\n");
	}
	if(unlink(depfile) < 0) {
		perror("unlink");
		fprintf(stderr, "tup error: Unable to unlink depfile: %s\n", depfile);
		return -1;
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

int server_postexec(struct server *s)
{
	char buf[64];
	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	if(unlinkat(tup_top_fd(), buf, 0) < 0) {
		server_lock(s);
		perror(buf);
		fprintf(stderr, "tup error: Unable to unlink sub-process output file.\n");
		server_unlock(s);
		return -1;
	}
	return 0;
}

int server_unlink(void)
{
	/* We don't have a sandbox for ldpreload, so errant files need to be
	 * unlinked.
	 */
	return 1;
}

int server_is_dead(void)
{
	return 0;
}

int server_config_start(struct server *s)
{
	/* Currently unused - this is only needed for symlink detection in fuse. */
	if(s) {}
	return 0;
}

int server_config_stop(struct server *s)
{
	if(s) {}
	return 0;
}

int server_parser_start(struct parser_server *ps)
{
	ps->root_fd = tup_top_fd();
	return 0;
}

int server_parser_stop(struct parser_server *ps)
{
	if(ps) {}
	return 0;
}

int server_run_script(FILE *f, tupid_t tupid, const char *cmdline,
		      struct tupid_entries *env_root, char **rules)
{
	if(f || tupid || cmdline || env_root || rules) {/* unsupported */}
	fprintf(stderr, "tup error: Run scripts are not yet supported on this platform.\n");
	return -1;
}

int server_symlink(struct server *s, const char *target, int dfd, const char *linkpath)
{
	if(s) {/* unused */}
	if(symlinkat(target, dfd, linkpath) < 0) {
		perror("symlinkat");
		fprintf(stderr, "tup error: unable to create symlink at '%s' pointing to target '%s'\n", linkpath, target);
		return -1;
	}
	return 0;
}

static int get_symlink(const char *filename, char **ret)
{
	char linkbuf[PATH_MAX];
	ssize_t linklen;
	linklen = readlink(filename, linkbuf, sizeof(linkbuf));
	if(linklen <= 0)
		return 0;
	linkbuf[linklen] = 0;
	if(linkbuf[0] == '/') {
		*ret = strdup(linkbuf);
		if(!*ret) {
			perror("strdup");
			return -1;
		}
	} else {
		char *last_slash;
		int dirlen;
		last_slash = strrchr(filename, '/');
		if(!last_slash) {
			fprintf(stderr, "tup error: Expected a '/' in the symlink filename: %s\n", filename);
			return -1;
		}
		dirlen = last_slash - filename + 1;
		*ret = malloc(dirlen + linklen + 1);
		if(!*ret) {
			perror("malloc");
			return -1;
		}
		strncpy(*ret, filename, dirlen);
		strcpy(*ret + dirlen, linkbuf);
	}
	return 0;
}

static int add_symlinks(const char *path, struct file_info *finfo)
{
	char *linkpath = NULL;

	if(get_symlink(path, &linkpath) < 0)
		return -1;
	if(linkpath) {
		if(handle_file(ACCESS_READ, linkpath, "", finfo) < 0) {
			fprintf(stderr, "tup error: Failed to call handle_file on a symlink event '%s'\n", linkpath);
			return -1;
		}
		free(linkpath);
	}
	return 0;
}

static int process_depfile(struct server *s, int fd)
{
	char event1[PATH_MAX];
	char event2[PATH_MAX];

	while(1) {
		struct access_event event;
		int rc;

		rc = read(fd, &event, sizeof(event));
		if(rc == 0) {
			break;
		} else if(rc != sizeof(event)) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read the access_event structure from the dependency file.\n");
			return -1;
		}

		if(!event.len)
			continue;

		if(event.len >= PATH_MAX - 1) {
			fprintf(stderr, "tup error: Size of %i bytes is longer than the max filesize\n", event.len);
			return -1;
		}
		if(event.len2 >= PATH_MAX - 1) {
			fprintf(stderr, "tup error: Size of %i bytes is longer than the max filesize\n", event.len2);
			return -1;
		}

		if(read(fd, &event1, event.len + 1) != event.len + 1) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read the first event from the dependency file.\n");
			return -1;
		}
		if(read(fd, &event2, event.len2 + 1) != event.len2 + 1) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read the second event from the dependency file.\n");
			return -1;
		}

		if(event1[event.len] != '\0' || event2[event.len2] != '\0') {
			fprintf(stderr, "tup error: Missing null terminator in access_event\n");
			return -1;
		}

		if(event.at == ACCESS_WRITE) {
			struct mapping *map;

			map = malloc(sizeof *map);
			if(!map) {
				perror("malloc");
				return -1;
			}
			map->realname = strdup(event1);
			if(!map->realname) {
				perror("strdup");
				return -1;
			}
			map->tmpname = strdup(event1);
			if(!map->tmpname) {
				perror("strdup");
				return -1;
			}
			map->tent = NULL; /* This is used when saving deps */
			LIST_INSERT_HEAD(&s->finfo.mapping_list, map, list);
		}
		if(handle_file(event.at, event1, event2, &s->finfo) < 0) {
			fprintf(stderr, "tup error: Failed to call handle_file on event '%s'\n", event1);
			return -1;
		}
		if(event.at == ACCESS_READ) {
			if(add_symlinks(event1, &s->finfo) < 0)
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
