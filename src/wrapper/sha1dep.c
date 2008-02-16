#include "sha1dep.h"
#include "mkdirhier.h"
#include "tupid.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

/** Enough space for the sha1 hash in ascii hex */
#define SHA1_X "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

static int test_and_set_index(const char *file);

int write_sha1dep(const char *file, const char *depends_on)
{
	struct stat buf;
	int rc;
	char depfilename[] = ".tup/" SHA1_X "/" SHA1_X;

	if(test_and_set_index(file) < 0)
		return -1;
	if(test_and_set_index(depends_on) < 0)
		return -1;

	tupid_from_filename(depfilename + 5, depends_on);
	tupid_from_filename(depfilename + 46, file);

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

	rc = creat(depfilename, 0666);
	if(rc < 0) {
		perror("creat");
		return -1;
	}
	close(rc);
	return 0;
}

static int test_and_set_index(const char *file)
{
	/* TODO: Not multi-process safe since multiple procs may try to read/
	 * write to tup.name simultaneously? maybe write to tup.name.pid? or
	 * put a lock in .tup/lock or somesuch?
	 */
	static char read_filename[MAXPATHLEN];
	int fd;
	int rc;
	int len;
	char tupfilename[] = ".tup/" SHA1_X "/tup.name";

	tupid_from_filename(tupfilename + 5, file);
	if(mkdirhier(tupfilename) < 0)
		return -1;
	fd = open(tupfilename, O_RDONLY);
	if(fd < 0) {
		fd = open(tupfilename, O_WRONLY | O_CREAT, 0666);
		if(fd < 0) {
			perror("open");
			return -1;
		}
		len = strlen(file);
		rc = write(fd, file, len);
		if(rc < 0) {
			perror("write");
			return -1;
		}
		if(rc != len) {
			fprintf(stderr, "Unable to write all %i bytes to %s.\n",
				len, tupfilename);
		}
	} else {
		rc = read(fd, read_filename, sizeof(read_filename) - 1);
		if(rc < 0) {
			perror("read");
			return -1;
		}
		read_filename[rc] = 0;
		if(strcmp(read_filename, file) != 0) {
			fprintf(stderr, "Gak! SHA1 collision? Requested "
				"file '%s' doesn't match stored file '%s' for "
				"in '%s'\n", file, read_filename, tupfilename);
			return -1;
		}
	}
	close(fd);
	return 0;
}
