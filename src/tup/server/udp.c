#include "tup/server.h"
#include "tup/file.h"
#include "tup/debug.h"
#include "tup/fileio.h"
#include "tup/db.h"
#include "tup/graph.h"
#include "tup/entry.h"
#include "dllinject/dllinject.h"
#include "compat/win32/dirpath.h"
#include "compat/dir_mutex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

static int start_server(struct server *s);
static int stop_server(struct server *s);
static void *message_thread(void *arg);
static int server_inited = 0;

int server_init(void)
{
	char *slash;
	char mycwd[PATH_MAX];

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

	return 0;
}

int server_quit(void)
{
	return 0;
}

#define SHSTR  "sh -c '"
#define CMDSTR "CMD.EXE /Q /C "
int server_exec(struct server *s, int vardict_fd, int dfd, const char *cmd,
		struct tup_entry *dtent)
{
	int rc = -1;
	DWORD return_code = 1;
	BOOL ret;
	PROCESS_INFORMATION pi;
	STARTUPINFOA sa;
	size_t namesz = strlen(cmd);
	size_t cmdsz = sizeof(CMDSTR) - 1;
	char* cmdline = (char*) __builtin_alloca(namesz + cmdsz + 1 + 1);

	int have_shell = strncmp(cmd, "sh ", 3) == 0
		|| strncmp(cmd, "cmd ", 4) == 0;

	int need_shell = strchr(cmd, '&') != NULL
		|| strchr(cmd, '|') != NULL
		|| strchr(cmd, '>') != NULL
		|| strchr(cmd, '<') != NULL
		|| strncmp(cmd, "./", 2) == 0;

	int need_sh = strncmp(cmd, "./", 2) == 0;

	if(vardict_fd) {}

	s->dt = dtent->tnode.tupid;
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

	memset(&sa, 0, sizeof(sa));
	sa.cb = sizeof(STARTUPINFOW);

	pi.hProcess = INVALID_HANDLE_VALUE;
	pi.hThread = INVALID_HANDLE_VALUE;

	pthread_mutex_lock(&dir_mutex);
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
		pthread_mutex_unlock(&dir_mutex);
		goto end;
	}
	ret = CreateProcessA(
		NULL,
		cmdline,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&sa,
		&pi);
	pthread_mutex_unlock(&dir_mutex);

	if(!ret) {
		fprintf(stderr, "tup error: failed to create child process: %s\n", strerror(errno));
		goto end;
	}

	if(tup_inject_dll(&pi, s->udp_port)) {
		fprintf(stderr, "tup error: failed to inject dll: %s\n", strerror(errno));
		goto end;
	}

	if(ResumeThread(pi.hThread) == (DWORD)~0) {
		fprintf(stderr, "tup error: failed to start thread: %s\n", strerror(errno));
		goto end;
	}

	if(WaitForSingleObject(pi.hThread, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "tup error: failed to wait for thread: %s\n", strerror(errno));
		goto end;
	}

	if(WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "tup error: failed to wait for process: %s\n", strerror(errno));
		goto end;
	}

	if(!GetExitCodeProcess(pi.hProcess, &return_code)) {
		fprintf(stderr, "tup error: failed to get exit code: %s\n", strerror(errno));
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

int server_is_dead(void)
{
	/* No signals in win32? */
	return 0;
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
	close(s->sd[0]);
	close(s->sd[1]);
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
			list_add(&map->list, &s->finfo.mapping_list);
		}
		if(handle_file(event->at, event1, event2, &s->finfo, s->dt) < 0) {
			fprintf(stderr, "message_thread end\n");
			return (void*)-1;
		}
	}
	return NULL;
}
