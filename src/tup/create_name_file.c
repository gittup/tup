#include "fileio.h"
#include "debug.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int create_name_file(const char *file)
{
	return create_name_file2(file, "");
}

int create_name_file2(const char *path, const char *file)
{
	int fd;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";
	char depfilename[] = ".tup/object/" SHA1_XD "/.secondary";
	tupid_t tupid;

	path = tupid_from_path_filename(tupid, path, file);
	tupid_to_xd(tupfilename + 12, tupid);
	tupid_to_xd(depfilename + 12, tupid);

	DEBUGP("create name file '%s' containing '%s%s'.\n",
	       tupfilename, path, file);

	tupfilename[13 + sizeof(tupid_t)] = 0;
	if(mkdir(tupfilename, 0777) < 0) {
		/* If the dir already exists, we assume we're just overwriting
		 * the name file, so we don't make the 'create' link.
		 */
		if(errno != EEXIST) {
			perror("mkdir");
			return -1;
		}
	} else {
		if(create_tup_file_tupid("create", tupid) < 0)
			return -1;
	}
	tupfilename[13 + sizeof(tupid_t)] = '/';

	fd = open(tupfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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
	if(delete_tup_file("delete", tupid) < 0)
		goto err_out;
	close(fd);
	fd = open(depfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(depfilename);
		return -1;
	}
	close(fd);
	return 0;

err_out:
        close(fd);
        return -1;
}
