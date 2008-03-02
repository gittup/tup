#include "fileio.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int link_if_not_exist(const char *src, const char *dest)
{
	struct stat buf;
	int rc;

	rc = stat(dest, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", dest);
			return -1;
		}
	}

	rc = link(src, dest);
	if(rc < 0) {
		perror(dest);
		return -1;
	}
	return 0;
}
