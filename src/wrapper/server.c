#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "access_event.h"
#include "debug.h"
#include "file.h"

static int sd = -1;
static struct sockaddr_un addr;
static pthread_t tid;
static void *message_thread(void *arg);
static void sighandler(int sig);
static struct sigaction sigact = {
	.sa_handler = sighandler,
	.sa_flags = 0,
};

int start_server(void)
{
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
	/* TODO: Permanent-ize this somehow? same directory as wrapper? */
	setenv("LD_PRELOAD", "/home/mjs/tup/ldpreload.so", 1);
	DEBUGP("Started server '%s'\n", addr.sun_path);

	return 0;
}

void stop_server(void)
{
	if(sd != -1) {
		enum access_type at = ACCESS_STOP_SERVER;
		DEBUGP("Stopping server '%s'\n", addr.sun_path);
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
	static struct access_event event;
	int rc;
	if(arg) {/* unused */}

	while((rc = recv(sd, &event, sizeof(event), 0)) > 0) {
		if(event.at == ACCESS_STOP_SERVER)
			break;
		if(handle_file(&event) < 0)
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
