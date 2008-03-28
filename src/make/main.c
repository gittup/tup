#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/flist.h"
#include "tup/slurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int create(const tupid_t tupid);
int update(const tupid_t tupid, char type);

int create(const tupid_t tupid)
{
	int pid;
	int status;
	struct buf name;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(tupfilename + 12, tupid);
	if(slurp(tupfilename, &name) < 0) {
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
		execl("/usr/bin/make", "make", "--no-print-directory", "-r", "-R", "-C", name.s, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0)
			return 0;
		fprintf(stderr, "Error: Update process failed with %i\n",
			WEXITSTATUS(status));
		return -WEXITSTATUS(status);
	}
	fprintf(stderr, "Error: Update process didn't return.\n");
	return -1;
}

int update(const tupid_t tupid, char type)
{
	int pid;
	int status;
	struct buf name;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(tupfilename + 12, tupid);
	if(slurp(tupfilename, &name) < 0) {
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
		if(type & TUP_CREATE) {
			execl("/usr/bin/make", "make", "--no-print-directory", "-r", "-R", "TUP_CREATE=1", name.s, NULL);
			perror("execl");
			exit(1);
		}
		execl("/usr/bin/make", "make", "--no-print-directory", "-r", "-R", name.s, NULL);
		perror("execl");
		exit(1);
	}
	wait(&status);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0)
			return 0;
		fprintf(stderr, "Error: Update process failed with %i\n",
			WEXITSTATUS(status));
		return -WEXITSTATUS(status);
	}
	fprintf(stderr, "Error: Update process didn't return.\n");
	return -1;
}
