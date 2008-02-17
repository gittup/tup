#include "fsdep.h"
#include "mkdirhier.h"
#include "debug.h"
#include "tup-compat.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

static int create_file(char *filename);

int write_fsdep(const char *file, const char *depends_on)
{
	/* In make this would be written as:
	 *  file: depends_on
	 * eg:
	 *  foo.o: foo.c
	 */
	static char tupd[PATH_MAX];

	if(snprintf(tupd, sizeof(tupd), ".tup/%s.tupd/%s", depends_on, file) >=
	   (signed)sizeof(tupd)) {
		fprintf(stderr, "Filename (.tup/%s.tupd/%s) too long\n",
			depends_on, file);
		return -1;
	}

	create_file(tupd);
	return 0;
}

static int create_file(char *filename)
{
	struct stat buf;
	int rc;
	int fd;

	DEBUGP("Create file: '%s'\n", filename);

	/* Quick check to see if the file already exists. */
	rc = stat(filename, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", filename);
			return -1;
		}
	}

	if(mkdirhier(filename) < 0) {
		return -1;
	}

	fd = creat(filename, 0666);
	if(fd < 0) {
		perror("creat");
		return -1;
	}
	close(fd);
	return 0;
}
