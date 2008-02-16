#include "mkdirhier.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int __mkdirhier(char *dirname);

int mkdirhier(char *filename)
{
	int rc = 0;
	char *slash;
	char *tmp;

	tmp = filename;
	do {
		slash = strchr(tmp, '/');
		if(slash == NULL)
			break;

		tmp = slash+1;
		*slash = 0;
		rc = __mkdirhier(filename);
		*slash = '/';
	} while(rc == 0);

	return rc;
}

static int __mkdirhier(char *dirname)
{
	int rc;
	struct stat buf;

	DEBUGP("Mkdirhier: '%s'\n", dirname);
	rc = stat(dirname, &buf);
	if(rc == 0) {
		if(!S_ISDIR(buf.st_mode)) {
			fprintf(stderr, "Error: '%s' is not a directory.\n",
				dirname);
			return -1;
		}
	} else {
		if(mkdir(dirname, 0777) < 0) {
			perror("mkdir");
			return -1;
		}
	}
	return 0;
}
