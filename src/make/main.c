#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/flist.h"
#include "tup/slurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int create(const tupid_t tupid);

int create(const tupid_t tupid)
{
	int pid;
	int status;
	struct buf name;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(tupfilename + 12, tupid);
	if(slurp(tupfilename, &name) < 0) {
		perror(tupfilename);
		return -1;
	}
	name.s[name.len-1] = 0;

	pid = fork();
	if(pid < 0) {
		perror("fork");
		return -1;
	}
	if(pid == 0) {
		clearenv();
		setenv("PATH", "/bin:/usr/bin:/home/marf/tup", 1); /* TODO */
		setenv("TUPWD", name.s, 1);
		execl("/usr/bin/make", "make", "--no-print-directory", "-r", "-R", "-C", name.s, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			free(name.s);
			return 0;
		}
		fprintf(stderr, "Error: Update process failed with %i in %s/\n",
			WEXITSTATUS(status), name.s);
	} else {
		fprintf(stderr, "Error: Update process didn't return at %s/\n",
			name.s);
	}
	free(name.s);
	return -1;
}
