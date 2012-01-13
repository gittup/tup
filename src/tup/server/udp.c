/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
 * Copyright (C) 2010-2012  Mike Shal <marfey@gmail.com>
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
#include "dllinject/dllinject.h"
#include "compat/win32/dirpath.h"
#include "compat/win32/open_notify.h"
#include "compat/dir_mutex.h"
#include <stdio.h>
#include <errno.h>

#define TUP_TMP ".tup/tmp"

static int start_server(struct server *s);
static int stop_server(struct server *s);
static void *message_thread(void *arg);
static int server_inited = 0;

int server_pre_init(void)
{
	return 0;
}

int server_post_exit(void)
{
	return 0;
}

int server_init(enum server_mode mode)
{
	char *slash;
	char mycwd[PATH_MAX];
	struct flist f = {0, 0, 0};

	if(mode) {/* unused */}

	if(server_inited)
		return 0;

	if (GetModuleFileNameA(NULL, mycwd, PATH_MAX - 1) == 0)
		return -1;

	mycwd[PATH_MAX - 1] = '\0';
	slash = strrchr(mycwd, '\\');
	if (slash) {
		*slash = '\0';
	}

	tup_inject_setexecdir(mycwd);
	server_inited = 1;

	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return -1;
	}
	if(mkdir(TUP_TMP) < 0) {
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
	return 0;
}

/* This runs with the dir_mutex taken. We need to make sure that we:
 * 1) Create the output file with the inherit flag set
 * 2) We start the new process to inherit that file
 * 3) We close down the file so that no other process inherits it (ie: if we
 *    are running in parallel - we don't want anyone else to get our output file
 *    handle)
 */
static int create_process(struct server *s, int dfd, char *cmdline,
			  struct tup_env *newenv,
			  PROCESS_INFORMATION *pi)
{
	STARTUPINFOA sa;
	SECURITY_ATTRIBUTES sec;
	BOOL ret;
	char buf[64];

	memset(&sa, 0, sizeof(sa));
	sa.cb = sizeof(STARTUPINFOW);

	memset(&sec, 0, sizeof(sec));
	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

	if(chdir(win32_get_dirpath(tup_top_fd())) < 0) {
		perror("chdir");
		fprintf(stderr, "tup error: Unable to chdir to the project root directory to create a temporary output file.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), ".tup\\tmp\\output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	sa.hStdOutput = CreateFile(buf, GENERIC_WRITE, 0, &sec, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(sa.hStdOutput == INVALID_HANDLE_VALUE) {
		perror(buf);
		fprintf(stderr, "tup error: Unable to create temporary file for stdout\n");
		return -1;
	}
	sa.hStdError = sa.hStdOutput;
	sa.dwFlags = STARTF_USESTDHANDLES;

	pi->hProcess = INVALID_HANDLE_VALUE;
	pi->hThread = INVALID_HANDLE_VALUE;

	/* Passing in the directory to lpCurrentDirectory is insufficient
	 * because the command may run as './foo.exe', so we need to change to
	 * the correct directory before calling CreateProcessA. This may just
	 * happen to work in most cases because the unlinkat() called to remove
	 * the outputs will usually change to the correct directory anyway.
	 * This isn't necessarily the case if the command has no outputs, and
	 * also wouldn't be synchronized.
	 */
	if(chdir(win32_get_dirpath(dfd))) {
		fprintf(stderr, "tup error: Unable to change working directory to '%s'\n", win32_get_dirpath(dfd));
		return -1;
	}
	ret = CreateProcessA(
		NULL,
		cmdline,
		NULL,
		NULL,
		TRUE,
		CREATE_SUSPENDED,
		newenv->envblock,
		NULL,
		&sa,
		pi);
	CloseHandle(sa.hStdOutput);

	if(!ret)
		return -1;
	return 0;
}

#define SHSTR  "sh -c '"
#define CMDSTR "CMD.EXE /Q /C "
int server_exec(struct server *s, int dfd, const char *cmd, struct tup_env *newenv,
		struct tup_entry *dtent)
{
	int rc = -1;
	int proc_rc;
	DWORD return_code = 1;
	PROCESS_INFORMATION pi;
	size_t namesz = strlen(cmd);
	size_t cmdsz = sizeof(CMDSTR) - 1;
	char* cmdline = (char*) __builtin_alloca(namesz + cmdsz + 1 + 1);
	char buf[64];

	int have_shell = strncmp(cmd, "sh ", 3) == 0
		|| strncmp(cmd, "cmd ", 4) == 0;

	int need_shell = strchr(cmd, '&') != NULL
		|| strchr(cmd, '|') != NULL
		|| strchr(cmd, '>') != NULL
		|| strchr(cmd, '<') != NULL
		|| strncmp(cmd, "./", 2) == 0;

	int need_sh = strncmp(cmd, "./", 2) == 0;
	if(dtent) {}

	if(start_server(s) < 0) {
		fprintf(stderr, "Error starting update server.\n");
		return -1;
	}

	cmdline[0] = '\0';
	/* Only pull in cmd if really necessary */
	if(!have_shell && need_shell) {
		if(need_sh) {
			strcat(cmdline, SHSTR);
		} else {
			strcat(cmdline, CMDSTR);
		}
	} else {
		need_sh = 0;
	}
	strcat(cmdline, cmd);
	if(need_sh) {
		strcat(cmdline, "'");
	}

	pthread_mutex_lock(&dir_mutex);
	proc_rc = create_process(s, dfd, cmdline, newenv, &pi);
	pthread_mutex_unlock(&dir_mutex);

	if(proc_rc < 0) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to create child process: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	if(tup_inject_dll(&pi, s->udp_port)) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to inject dll: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	if(ResumeThread(pi.hThread) == (DWORD)~0) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to start thread: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	if(WaitForSingleObject(pi.hThread, INFINITE) != WAIT_OBJECT_0) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to wait for thread: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	if(WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to wait for process: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	if(!GetExitCodeProcess(pi.hProcess, &return_code)) {
		pthread_mutex_lock(s->error_mutex);
		fprintf(stderr, "tup error: failed to get exit code: %s\n", strerror(errno));
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	s->output_fd = openat(tup_top_fd(), buf, O_RDONLY);
	if(s->output_fd < 0) {
		pthread_mutex_lock(s->error_mutex);
		perror(buf);
		fprintf(stderr, "tup error: Unable to open sub-process output file after the process completed.\n");
		pthread_mutex_unlock(s->error_mutex);
		goto end;
	}

	s->exited = 1;
	s->exit_status = return_code;
	rc = 0;

end:
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	if(stop_server(s) < 0) {
		return -1;
	}
	return rc;
}

int server_postexec(struct server *s)
{
	char buf[64];
	snprintf(buf, sizeof(buf), ".tup/tmp/output-%i", s->id);
	buf[sizeof(buf)-1] = 0;
	if(unlinkat(tup_top_fd(), buf, 0) < 0) {
		pthread_mutex_lock(s->error_mutex);
		perror(buf);
		fprintf(stderr, "tup error: Unable to unlink sub-process output file.\n");
		pthread_mutex_unlock(s->error_mutex);
		return -1;
	}
	return 0;
}

int server_is_dead(void)
{
	/* No signals in win32? */
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

int server_run_script(tupid_t tupid, const char *cmdline,
		      struct tupid_entries *env_root, char **rules)
{
	if(tupid || cmdline || env_root || rules) {/* unsupported */}
	fprintf(stderr, "tup error: Run scripts are not yet supported on this platform.\n");
	return -1;
}

static int connect_udp(int socks[2])
{
	int port;
	int sasz;
	struct sockaddr_in sa;
	socks[0] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socks[1] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(socks[0] < 0 || socks[1] < 0) {
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = 0;

	if(bind(socks[0], (struct sockaddr*) &sa, sizeof(struct sockaddr_in))) {
		goto err;
	}

	sasz = sizeof(struct sockaddr_in);
	if(getsockname(socks[0], (struct sockaddr*) &sa, &sasz) || sasz != sizeof(struct sockaddr_in)) {
		goto err;
	}

	port = ntohs(sa.sin_port);

	if(connect(socks[1], (struct sockaddr*) &sa, sizeof(struct sockaddr_in))) {
		goto err;
	}

	return port;

err:
	close(socks[1]);
	close(socks[0]);
	return -1;
}

static int start_server(struct server *s)
{
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);

	s->udp_port = connect_udp(s->sd);
	if(s->udp_port < 0) {
		fprintf(stderr, "Failed to open UDP port\n");
		return -1;
	}

	init_file_info(&s->finfo);

	if(pthread_create(&s->tid, NULL, &message_thread, s) < 0) {
		perror("pthread_create");
		close(s->sd[0]);
		close(s->sd[1]);
		return -1;
	}

	return 0;
}

static int stop_server(struct server *s)
{
	void *retval = NULL;
	struct access_event e;
	int rc;

	memset(&e, 0, sizeof(e));
	e.at = ACCESS_STOP_SERVER;

	rc = send(s->sd[1], (const char*) &e, sizeof(e), 0);
	if(rc != sizeof(e)) {
		perror("send");
		return -1;
	}
	pthread_join(s->tid, &retval);
	if(close(s->sd[0]) < 0) {
		perror("close(s->sd[0])");
		return -1;
	}
	if(close(s->sd[1]) < 0) {
		perror("close(s->sd[1])");
		return -1;
	}
	s->sd[0] = INVALID_SOCKET;
	s->sd[1] = INVALID_SOCKET;

	WSACleanup();

	if(retval == NULL)
		return 0;
	return -1;
}

static void *message_thread(void *arg)
{
	int recvd;
	char buf[ACCESS_EVENT_MAX_SIZE];
	struct server *s = arg;

	while(1) {
		struct access_event* event = (struct access_event*) buf;
		char *event1, *event2;

		recvd = recv(s->sd[0], buf, sizeof(buf), 0);
		if(recvd < (int) sizeof(struct access_event)) {
			perror("recv");
			break;
		}

		if(event->at == ACCESS_STOP_SERVER)
			break;

		if(event->at > ACCESS_STOP_SERVER) {
			fprintf(stderr, "Error: Received unknown access_type %d\n", event->at);
			return (void*)-1;
		}

		if(!event->len)
			continue;

		if(event->len >= PATH_MAX - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event->len);
			return (void*)-1;
		}
		if(event->len2 >= PATH_MAX - 1) {
			fprintf(stderr, "Error: Size of %i bytes is longer than the max filesize\n", event->len2);
			return (void*)-1;
		}

		event1 = (char*) event + sizeof(struct access_event);
		event2 = event1 + event->len + 1;

		if(recvd != (int) sizeof(struct access_event) + event->len + event->len2 + 2) {
			fprintf(stderr, "Error: Received weird size in access_event\n");
			return (void*)-1;
		}

		if(event1[event->len] != '\0' || event2[event->len2] != '\0') {
			fprintf(stderr, "Error: Missing null terminator in access_event\n");
			return (void*)-1;
		}

		if(event->at == ACCESS_WRITE) {
			struct mapping *map;

			map = malloc(sizeof *map);
			if(!map) {
				perror("malloc");
				return (void*)-1;
			}
			map->realname = strdup(event1);
			if(!map->realname) {
				perror("strdup");
				return (void*)-1;
			}
			map->tmpname = strdup(event1);
			if(!map->tmpname) {
				perror("strdup");
				return (void*)-1;
			}
			map->tent = NULL; /* This is used when saving deps */
			LIST_INSERT_HEAD(&s->finfo.mapping_list, map, list);
		}
		if(handle_file(event->at, event1, event2, &s->finfo) < 0) {
			fprintf(stderr, "message_thread end\n");
			return (void*)-1;
		}
	}
	return NULL;
}
