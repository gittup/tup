#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
/* TODO */
#include "../ldpreload/access_event.h"
#include "file.h"

static int sd = -1;
static struct sockaddr_un addr;
static pthread_t tid;
static void *message_thread(void *arg);

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
	setenv("LD_PRELOAD", "/home/mjs/tup/ldpreload/ldpreload.so", 1);
	fprintf(stderr, "Started server '%s'\n", addr.sun_path);

	return 0;
}

void stop_server(void)
{
	if(sd != -1) {
		enum rw_type rw = STOP_SERVER;
		fprintf(stderr, "Stopping server '%s'\n", addr.sun_path);
		/* TODO: ok to reuse sd here? */
		sendto(sd, &rw, sizeof(rw), 0, (void*)&addr, sizeof(addr));
		pthread_join(tid, NULL);
		write_files();
		close(sd);
		unlink(addr.sun_path);
		unsetenv(SERVER_NAME);
	}
}

static void *message_thread(void *arg)
{
	static struct access_event event;
	int rc;
	if(arg) {/* unused */}

	while((rc = recv(sd, &event, sizeof(event), 0)) > 0) {
		fprintf(stderr, "Recv %i bytes.\n", rc);
		if(event.rw == STOP_SERVER)
			break;
		handle_file(&event);
	}
	if(rc < 0) {
		perror("recv");
	}
	return NULL;
}
