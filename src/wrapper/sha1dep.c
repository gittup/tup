#include "sha1dep.h"
#include "mkdirhier.h"
#include "tup-compat.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int write_sha1dep(const tupid_t file, const tupid_t depends_on)
{
	struct stat buf;
	int rc;
	char depfilename[] = ".tup/" SHA1_X "/" SHA1_X;

	memcpy(depfilename + 5, depends_on, sizeof(tupid_t));
	memcpy(depfilename + 46, file, sizeof(tupid_t));

	rc = stat(depfilename, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", depfilename);
			return -1;
		}
	}

	DEBUGP("create dependency: %s\n", depfilename);
	mkdirhier(depfilename);

	rc = creat(depfilename, 0666);
	if(rc < 0) {
		perror("creat");
		return -1;
	}
	close(rc);
	return 0;
}
