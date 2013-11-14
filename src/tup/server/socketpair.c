#include "tup/server.h"
#include "tup/file.h"
#include "tup/debug.h"
#include "tup/getexecwd.h"
#include "tup/db.h"
#include "tup/lock.h"
#include "tup/config.h"
#include "tup/entry.h"
#include "tup/environ.h"
#include "tup/flist.h"
#include "tup/variant.h"
#include "compat/open_notify.h"
#include "master_fork.h"
#include "process_depfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

static int server_setenv(struct execmsg *em, const char *deps, struct tup_env *env);
static void sighandler(int sig);

static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = SA_RESTART,
};
static int sig_quit = 0;
static char ldpreload_path[PATH_MAX];

int server_init(enum server_mode mode)
{
	struct flist f = {0, 0, 0};

	if(mode) {/* TODO */}
	if(snprintf(ldpreload_path, sizeof(ldpreload_path),
		    "%s/tup-ldpreload.so", getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for tup-ldpreload.so library is "
				"too long.\n");
		return -1;
	}
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		fprintf(stderr, "tup error: Unable to change to tup root directory.\n");
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
				fprintf(stderr, "tup error: Unable to clean out a file in .tup/tmp directory. Please try cleaning this directory manually.\n");
				return -1;
			}
		}
	}

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}

	return 0;
}

int server_quit(void)
{
	printf("TODO: Server quit\n");
	return 0;
}

int server_parser_start(struct parser_server *ps)
{
	ps->root_fd = tup_top_fd();
	if(open_notify_push(&ps->s.finfo) < 0)
		return -1;
	return 0;
}

int server_parser_stop(struct parser_server *ps)
{
	if(open_notify_pop(&ps->s.finfo) < 0)
		return -1;
	return 0;
}

int server_run_script(FILE *f, tupid_t tupid, const char *cmdline,
		      struct tupid_entries *env_root, char **rules)
{
	if(f || tupid || env_root || rules) {}
	printf("TODO: Run script %s\n", cmdline);
	return -1;
}

int server_postexec(struct server *s)
{
	if(s) {}
	return 0;
}

int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent, int full_deps)
{
	int status;
	char buf[64];
	char *job = NULL;
	char dir[PATH_MAX];
	char deps[PATH_MAX];
	struct execmsg em;
	struct variant *variant;
	int depsfd;

	if(s || dfd || newenv || dtent || full_deps) {/* TODO */}

	memset(&em, 0, sizeof(em));
	em.sid = s->id;
	em.single_output = 1;
	em.do_chroot = 0;
	em.joblen = 0;

	if(snprintf(deps, sizeof(deps), "%s/.tup/tmp/deps-%i", get_tup_top(), s->id) >= (int)sizeof(deps)) {
		fprintf(stderr, "tup error: deps file sized incorrectly.\n");
		return -1;
	}
	depsfd = open(deps, O_CREAT | O_TRUNC | O_RDONLY, 0666);
	if(depsfd < 0) {
		perror(deps);
		fprintf(stderr, "tup error: Unable to create dependency file '%s'\n", deps);
		return -1;
	}

	if(server_setenv(&em, deps, newenv) < 0)
		return -1;

	/* dirlen includes the \0, which snprintf does not count. Hence the -1/+1
	 * adjusting.
	 */
	strncpy(dir, get_tup_top(), sizeof(dir));
	em.dirlen = get_tup_top_len();
	em.dirlen += snprint_tup_entry(dir + em.dirlen,
			sizeof(dir) - em.dirlen - 1,
			dtent) + 1;
	if(em.joblen >= JOB_MAX || em.dirlen >= PATH_MAX) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: Directory for tup entry %lli is too long.\n", dtent->tnode.tupid);
		print_tup_entry(stderr, dtent);
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}
	em.cmdlen = strlen(cmd) + 1;
	variant = tup_entry_variant(dtent);
	em.vardictlen = variant->vardict_len;
	if(master_fork_exec(&em, job, dir, cmd, newenv->envblock, variant->vardict_file, &status) < 0) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: Unable to fork sub-process.\n");
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}

	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	s->output_fd = openat(tup_top_fd(), buf, O_RDONLY);
	if(s->output_fd < 0) {
		pthread_mutex_lock(s->error_mutex);
		perror(buf);
		fprintf(stderr, "tup error: Unable to open sub-process output file.\n");
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}
	if(unlinkat(tup_top_fd(), buf, 0) < 0) {
		pthread_mutex_lock(s->error_mutex);
		perror(buf);
		fprintf(stderr, "tup error: Unable to unlink sub-process output file.\n");
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}

	if(WIFEXITED(status)) {
		s->exited = 1;
		s->exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		s->signalled = 1;
		s->exit_sig = WTERMSIG(status);
	} else {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: Expected exit status to be WIFEXITED or WIFSIGNALED. Got: %i\n", status);
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}

	if(process_depfile(s, depsfd) < 0)
		return -1;
	return 0;
}

static int server_setenv(struct execmsg *em, const char *deps, struct tup_env *env)
{
	char extrablock[PATH_MAX];
	unsigned int size;
	char *newenv;
	int newblocksize;
	int num_entries;

	size = snprintf(extrablock, sizeof(extrablock),
			"%s=%s%c"
			"DYLD_INSERT_LIBRARIES=%s",
			TUP_SERVER_NAME, deps, 0,
			ldpreload_path);
	/* The 3 new entries are the deps file, and 2 DYLD_* variables.  */
	num_entries = 3;

	if (size >= sizeof(extrablock)) {
		fprintf(stderr, "tup error: extra environment block sized incorrectly.\n");
		return -1;
	}
	/* Account for last \0 */
	size += 1;

	newblocksize = size + env->block_size;
	newenv = malloc(newblocksize);
	if(!newenv) {
		perror("malloc");
		return -1;
	}
	memcpy(newenv, extrablock, size);
	memcpy(newenv + size, env->envblock, env->block_size);
	free(env->envblock);
	env->envblock = newenv;
	env->block_size = newblocksize;
	env->num_entries += num_entries;

	em->envlen = env->block_size;
	em->num_env_entries = env->num_entries;
	return 0;
}

int server_is_dead(void)
{
	return sig_quit;
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
	}
}
