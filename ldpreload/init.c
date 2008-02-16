#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "file.h"

static void preload_init(void) __attribute__((constructor));
static void preload_exit(void) __attribute__((destructor));

static void preload_init(void)
{
	char *server_name;
	server_name = getenv("tup_master");
	fprintf(stderr, "[31mMARF[%i]: Init[0m\n", getpid());
	if(!server_name) {
		if(start_server() < 0)
			exit(1);
		setenv("tup_master", get_server_name(), 0);
	} else {
		set_server_name(server_name);
	}
}

static void preload_exit(void)
{
	if(is_server()) {
		stop_server();
	}
	fprintf(stderr, "[31mMARF[%i]: Exit[0m\n", getpid());
}
