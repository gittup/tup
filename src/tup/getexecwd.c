#define _BSD_SOURCE
#include "getexecwd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "compat.h"

static char mycwd[PATH_MAX];
static int check_path(const char *path, const char *file);

int init_getexecwd(const char *argv0)
{
	char *slash;
	int curfd;
	int rc = -1;

	strcpy(mycwd, argv0);
	slash = strrchr(mycwd, '/');
	if(slash) {
		/* Relative and absolute paths */
		curfd = open(".", O_RDONLY);
		if(curfd < 0) {
			perror(".");
			return -1;
		}
		*slash = 0;
		if(chdir(mycwd) < 0) {
			perror("chdir");
			goto out_err;
		}
		if(getcwd(mycwd, sizeof(mycwd)) == NULL) {
			perror("getcwd");
			goto out_err;
		}
		if(fchdir(curfd) < 0) {
			perror("fchdir");
			goto out_err;
		}
		rc = 0;
out_err:
		close(curfd);
	} else {
		/* Use $PATH */
		char *path;
		char *colon;
		char *p;

		path = getenv("PATH");
		if(!path) {
			fprintf(stderr, "Unable to get PATH environment.\n");
			return -1;
		}

		p = path;
		while(rc == -1 && (colon = strchr(p, ':')) != NULL) {
			*colon = 0;
			rc = check_path(p, argv0);
			*colon = ':';
			p = colon + 1;
		}
		if(rc == -1)
			rc = check_path(p, argv0);
	}

	return rc;
}

const char *getexecwd(void)
{
	return mycwd;
}

static int check_path(const char *path, const char *file)
{
	struct stat buf;
	unsigned int len;

	len = snprintf(mycwd, sizeof(mycwd), "%s/%s", path, file);
	if(len >= sizeof(mycwd)) {
		fprintf(stderr, "Unable to fit path in mycwd buffer.\n");
		goto out_err;
	}
	if(stat(mycwd, &buf) < 0)
		goto out_err;
	if(S_ISREG(buf.st_mode)) {
		strcpy(mycwd, path);
		return 0;
	}
out_err:
	mycwd[0] = 0;
	return -1;
}
