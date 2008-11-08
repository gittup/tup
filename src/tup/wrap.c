#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include "server.h"
#include "file.h"
#include "debug.h"
#include "compat.h"
#include "tupid.h"
#include "wrap.h"

int wrap(int argc, char **argv)
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

	lock_fd = open(TUP_OBJECT_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_OBJECT_LOCK);
		return 1;
	}
	if(flock(lock_fd, LOCK_SH) < 0) {
		perror("flock");
		return 1;
	}

	start_server();
	pid = fork();
	if(pid < 0) {
		perror("fork");
		return 1;
	}
	if(pid == 0) {
		execvp(argv[arg_start], &argv[arg_start]);
		perror("execvp");
		return 1;
	}
	wait(&status);
	stop_server();

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			new_tupid_t cmdid;

			cmdid = atoll(getenv(TUP_CMD_ID));
			if(write_files(cmdid) < 0)
				return 1;
			return 0;
		}
		return WEXITSTATUS(status);
	}
	flock(lock_fd, LOCK_UN);
	fprintf(stderr, "Program terminated abnormally (%i)\n", status);
	return 1;
}
