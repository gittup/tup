#include "fileio.h"
#include "debug.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h> /* TODO */
#include <sys/stat.h>

int create_name_file(const char *path)
{
	int fd;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";
	char depfilename[] = ".tup/object/" SHA1_XD "/.secondary";
	tupid_t tupid;

	tupid_from_filename(tupid, path);
	tupid_to_xd(tupfilename + 12, tupid);
	tupid_to_xd(depfilename + 12, tupid);

	DEBUGP("create name file '%s' containing '%s'.\n",
	       tupfilename, path);

	tupfilename[13 + sizeof(tupid_t)] = 0;
	if(mkdir(tupfilename, 0777) < 0) {
		/* If the dir already exists, we assume we're just overwriting
		 * the name file, so we don't make the 'create' link.
		 */
		if(errno != EEXIST) {
			perror(tupfilename);
			return -1;
		}
	} else {
		/* TODO */
		char *p2;
		char *dir;
		p2 = strdup(path);
		if(!p2) {
			perror("strdup");
			return -1;
		}
		dir = dirname(p2);
		if(create_dir_file(dir) < 0)
			return -1;
		free(p2);
		if(create_tup_file("modify", path) < 0)
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

int create_command_file(const char *cmd)
{
	int fd;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.cmd";
	tupid_t tupid;

	tupid_from_filename(tupid, cmd);
	tupid_to_xd(tupfilename + 12, tupid);

	DEBUGP("create command file '%s' containing '%s'.\n",
	       tupfilename, cmd);

	tupfilename[13 + sizeof(tupid_t)] = 0;
	if(mkdir(tupfilename, 0777) < 0) {
		if(errno != EEXIST) {
			perror(tupfilename);
			return -1;
		}
	}
	tupfilename[13 + sizeof(tupid_t)] = '/';

	fd = open(tupfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	if(write_all(fd, cmd, strlen(cmd), tupfilename) < 0)
		goto err_out;
	if(write_all(fd, "\n", 1, tupfilename) < 0)
		goto err_out;
	if(delete_tup_file("delete", tupid) < 0)
		goto err_out;
	close(fd);
	return 0;

err_out:
        close(fd);
        return -1;
}

#if 0
int create_link_file(const char *l)
{
	int fd;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.link";
	tupid_t tupid;

	tupid_from_filename(tupid, l);
	tupid_to_xd(tupfilename + 12, tupid);

	DEBUGP("create link file '%s' containing '%s'.\n",
	       tupfilename, l);

	tupfilename[13 + sizeof(tupid_t)] = 0;
	if(mkdir(tupfilename, 0777) < 0) {
		if(errno != EEXIST) {
			perror(tupfilename);
			return -1;
		}
	}
	tupfilename[13 + sizeof(tupid_t)] = '/';

	fd = open(tupfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	if(write_all(fd, l, strlen(l), tupfilename) < 0)
		goto err_out;
	if(write_all(fd, "\n", 1, tupfilename) < 0)
		goto err_out;
	if(delete_tup_file("delete", tupid) < 0)
		goto err_out;
	close(fd);
	return 0;

err_out:
        close(fd);
        return -1;
}
#endif

int create_dir_file(const char *path)
{
	int fd;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";
	tupid_t tupid;

	tupid_from_filename(tupid, path);
	tupid_to_xd(tupfilename + 12, tupid);

	DEBUGP("create dir file '%s' containing '%s'.\n",
	       tupfilename, path);

	tupfilename[13 + sizeof(tupid_t)] = 0;
	if(mkdir(tupfilename, 0777) < 0) {
		if(errno != EEXIST) {
			perror(tupfilename);
			return -1;
		}
	}
	if(create_tup_file("create", path) < 0)
		return -1;
	tupfilename[13 + sizeof(tupid_t)] = '/';

	fd = open(tupfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	if(write_all(fd, path, strlen(path), tupfilename) < 0)
		goto err_out;
	if(write_all(fd, "\n", 1, tupfilename) < 0)
		goto err_out;
	if(delete_tup_file("delete", tupid) < 0)
		goto err_out;
	close(fd);
	return 0;

err_out:
        close(fd);
        return -1;
}
