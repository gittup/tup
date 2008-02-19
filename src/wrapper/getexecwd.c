#define _BSD_SOURCE
#include "getexecwd.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "tup-compat.h"

static char mycwd[PATH_MAX];

int init_getexecwd(const char *argv0)
{
	char *slash;
	int curfd = -1;

	strcpy(mycwd, argv0);
	slash = strrchr(mycwd, '/');
	if(slash) {
		curfd = open(".", O_RDONLY);
		if(curfd < 0) {
			perror("open");
			return -1;
		}
		*slash = 0;
		if(chdir(mycwd) < 0) {
			perror("chdir");
			goto out_err;
		}
	}
	if(getcwd(mycwd, sizeof(mycwd)) == NULL) {
		perror("getcwd");
		goto out_err;
	}
	if(slash) {
		if(fchdir(curfd) < 0) {
			perror("fchdir");
			goto out_err;
		}
		close(curfd);
	}

	return 0;
out_err:
	if(curfd != -1)
		close(curfd);
	return -1;
}

const char *getexecwd(void)
{
	return mycwd;
}
