#include "fileio.h"
#include "compat.h"
#include "debug.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int recreate_cmd_file(const tupid_t tupid)
{
	int fd;
	int wfd;
	int len;
	char depfilename[] = ".tup/object/" SHA1_XD "/.cmd";
	char newfilename[] = ".tup/object/" SHA1_XD "/.newcmd";
	char buf[1024];

	DEBUGP("recreate cmd file '%s'\n", depfilename);

	tupid_to_xd(depfilename + 12, tupid);
	tupid_to_xd(newfilename + 12, tupid);

	if((fd = open(depfilename, O_RDONLY)) < 0) {
		perror(depfilename);
		return -1;
	}
	if((wfd = creat(newfilename, 0666)) < 0) {
		perror(newfilename);
		goto out_err;
	}
	while((len = read(fd, buf, sizeof(buf))) > 0) {
		if(write(wfd, buf, len) != len) {
			perror("write");
			goto out_err2;
		}
	}
	if(len < 0) {
		perror("read");
		goto out_err2;
	}
	close(wfd);
	close(fd);

	if(unlink(depfilename) < 0) {
		perror(depfilename);
		return -1;
	}
	if(rename(newfilename, depfilename) < 0) {
		perror(newfilename);
		return -1;
	}

	return 0;
out_err2:
	close(wfd);
out_err:
	close(fd);
	return -1;
}
