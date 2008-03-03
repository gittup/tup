#include "fileio.h"
#include "debug.h"
#include "tup-compat.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int create_name_file(const char *file)
{
	return create_name_file2(file, "");
}

int create_name_file2(const char *path, const char *file)
{
	int fd;
	int rc = -1;
	int len;
	char tupfilename[] = ".tup/object/" SHA1_X "/.name";
	static char read_filename[PATH_MAX];

	path = tupid_from_path_filename(tupfilename + 12, path, file);

	DEBUGP("create tup file '%s' containing '%s%s'.\n",
	       tupfilename, path, file);
	if(mkdirhier(tupfilename) < 0)
		return -1;
        fd = open(tupfilename, O_RDONLY);
        if(fd < 0) {
                fd = open(tupfilename, O_WRONLY | O_CREAT, 0666);
                if(fd < 0) {
                        perror(tupfilename);
                        return -1;
                }
		if(write_all(fd, path, strlen(path), tupfilename) < 0)
			goto err_out;
		if(write_all(fd, file, strlen(file), tupfilename) < 0)
			goto err_out;
		if(write_all(fd, "\n", 1, tupfilename) < 0)
			goto err_out;
		if(create_tup_file_tupid("create", tupfilename+12) < 0)
			goto err_out;
        } else {
		int pathlen = strlen(path);

                len = read(fd, read_filename, sizeof(read_filename) - 1);
                if(len < 0) {
                        perror("read");
			goto err_out;
                }
                read_filename[len] = 0;

		if(memcmp(read_filename, path, pathlen) != 0 ||
		   memcmp(read_filename+pathlen, file, strlen(file)) != 0) {
                        fprintf(stderr, "Gak! SHA1 collision? Requested "
                                "file '%s' doesn't match stored file '%s' for "
                                "in '%s'\n", file, read_filename, tupfilename);
			goto err_out;
                }
        }
	if(delete_tup_file("delete", tupfilename+12) < 0)
		goto err_out;
	rc = 0;
err_out:
        close(fd);
        return rc;
}
