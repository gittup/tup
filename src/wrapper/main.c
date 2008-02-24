#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include "server.h"
#include "file.h"
#include "getexecwd.h"
#include "debug.h"
#include "tup-compat.h"

int main(int argc, char **argv)
{
	int pid;
	int arg_start = 1;
	int status;
	int lock_fd;

	if(argc < 2) {
		fprintf(stderr, "Usage: %s cmd [args]\n", argv[0]);
		return 1;
	}
	if(strcmp(argv[1], "-d") == 0) {
		setenv(TUP_DEBUG, "1", 1);
		debug_enable("tup_wrapper");
		arg_start++;
	}

	if(init_getexecwd(argv[0]) < 0) {
		fprintf(stderr, "Error: Unable to determine wrapper program's "
			"execution directory.\n");
		return 1;
	}

	lock_fd = open(TUP_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_LOCK);
		return 1;
	}
	if(flock(lock_fd, LOCK_SH) < 0) {
		perror("flock");
		return 1;
	}

	start_server();
	pid = fork();
	if(pid == 0) {
		execvp(argv[arg_start], &argv[arg_start]);
		perror("execlp");
		return 1;
	}
	wait(&status);
	stop_server();

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			write_files(argc - arg_start, argv + arg_start);
		}
		return WEXITSTATUS(status);
	}
	flock(lock_fd, LOCK_UN);
	fprintf(stderr, "Program terminated abnormally (%i)\n", status);
	return 1;
}
