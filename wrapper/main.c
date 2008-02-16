#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "server.h"

int main(int argc, char **argv)
{
	int pid;

	if(argc < 2) {
		fprintf(stderr, "Usage: %s cmd [args]\n", argv[0]);
		return 1;
	}
	start_server();
	pid = fork();
	if(pid == 0) {
		execvp(argv[1], &argv[1]);
		perror("execlp");
		return 1;
	}
	wait(NULL);
	stop_server();

	return 0;
}
