#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "server.h"
#include "file.h"
#include "debug.h"

int main(int argc, char **argv)
{
	int pid;
	int arg_start = 1;

	if(argc < 2) {
		fprintf(stderr, "Usage: %s cmd [args]\n", argv[0]);
		return 1;
	}
	if(strcmp(argv[1], "-d") == 0) {
		setenv(TUP_DEBUG, "1", 1);
		debug_enable("tup_wrapper");
		arg_start++;
	}

	start_server();
	pid = fork();
	if(pid == 0) {
		execvp(argv[arg_start], &argv[arg_start]);
		perror("execlp");
		return 1;
	}
	wait(NULL);
	stop_server();
	write_files();

	return 0;
}
