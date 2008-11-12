#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/flist.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int create(const char *dir);

int create(const char *dir)
{
	int pid;
	int status;

	pid = fork();
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(pid == 0) {
		clearenv();
		setenv("PATH", "/bin:/usr/bin:/home/marf/tup", 1); /* TODO */
		setenv("TUPWD", dir, 1);
		execl("/usr/bin/make", "make", "--no-print-directory", "-r", "-R", "-C", dir, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			return 0;
		}
		fprintf(stderr, "Error: Update process failed with %i in %s/\n",
			WEXITSTATUS(status), dir);
	} else {
		fprintf(stderr, "Error: Update process didn't return at %s/\n",
			dir);
	}
	return -1;
}
