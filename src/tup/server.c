#include "server.h"
#include "file.h"
#include "access_event.h"
#include "debug.h"
#include "getexecwd.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

static void *message_thread(void *arg);
static void sighandler(int sig);

static int sd = -1;
static struct sockaddr_un addr;
static pthread_t tid;
static char ldpreload_path[PATH_MAX];
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};

int start_server(void)
{
	if(snprintf(ldpreload_path, sizeof(ldpreload_path), "%s/ldpreload.so",
		    getexecwd()) >= (signed)sizeof(ldpreload_path)) {
		fprintf(stderr, "Error: path for ldpreload.so library is too "
			"long.\n");
		return -1;
	}

	sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if(sd < 0) {
		perror("socket");
		return -1;
	}
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path)-1, "/tmp/tup-%i",
		 getpid());
	addr.sun_path[sizeof(addr.sun_path)-1] = 0;

	sigemptyset(&sigact.sa_mask);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	if(bind(sd, (void*)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(sd);
		return -1;
	}

	if(pthread_create(&tid, NULL, message_thread, NULL) < 0) {
		perror("pthread_create");
		close(sd);
		unlink(addr.sun_path);
	}

	setenv(SERVER_NAME, addr.sun_path, 1);
	setenv("LD_PRELOAD", ldpreload_path, 1);
	DEBUGP("started server '%s'\n", addr.sun_path);

	return 0;
}

void stop_server(void)
{
	if(sd != -1) {
		enum access_type at = ACCESS_STOP_SERVER;
		DEBUGP("stopping server '%s'\n", addr.sun_path);
		/* TODO: ok to reuse sd here? */
		sendto(sd, &at, sizeof(at), 0, (void*)&addr, sizeof(addr));
		pthread_join(tid, NULL);
		close(sd);
		unlink(addr.sun_path);
		unsetenv(SERVER_NAME);
		sd = -1;
	}
}

static void *message_thread(void *arg)
{
	struct access_event event;
	static char filename[PATH_MAX];
	static char cname[PATH_MAX];
	int rc;
	int lastslash;
	tupid_t dt;
	tupid_t tupid;
	if(arg) {/* unused */}

	while((rc = recv(sd, &event, sizeof(event), 0)) > 0) {
		if(event.at == ACCESS_STOP_SERVER)
			break;
		if(!event.len)
			continue;
		rc = recv(sd, filename, sizeof(filename), 0);
		if(rc < 0) {
			perror("recv");
			return NULL;
		}
		if(rc != event.len) {
			fprintf(stderr, "Error: received %i bytes, expecting %i bytes.\n", rc, event.len);
			return NULL;
		}

		/* TODO: Re-use tup_file_mod here? */
		/* Skip the file if it's outside of our local tree */
		if(canonicalize(filename, cname, sizeof(cname),
				&lastslash) < 0)
			continue;
		if(lastslash == -1) {
			dt = create_dir_file(".");
			if(dt < 0)
				return NULL;
			tupid = create_name_file(dt, cname);
			if(tupid < 0)
				return NULL;
		} else {
			cname[lastslash] = 0;
			dt = create_dir_file(cname);
			if(dt < 0)
				return NULL;
			tupid = create_name_file(dt, &cname[lastslash+1]);
			if(tupid < 0)
				return NULL;
		}
		if(handle_file(&event, tupid) < 0)
			break;
	}
	if(rc < 0) {
		perror("recv");
	}
	return NULL;
}

static void sighandler(int sig)
{
	/* Ensure the socket file is cleaned up if a signal is caught. */
	close(sd);
	unlink(addr.sun_path);
	exit(sig);
}
