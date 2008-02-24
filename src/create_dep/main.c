/* Test program to create dependencies by opening N files for read and one file
 * for write. This should be wrapped by the wrapper program.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int fd1;
	int fd2;
	int x;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s read_file write_file\n", argv[0]);
		return 1;
	}

	for(x=1; x<argc-1; x++) {
		fd1 = open(argv[x], O_RDONLY);
		if(fd1 < 0) {
			perror(argv[x]);
			return 1;
		}
		close(fd1);
	}
	fd2 = open(argv[argc-1], O_WRONLY);
	if(fd2 < 0) {
		perror(argv[argc-1]);
		return 1;
	}
	close(fd2);
	return 0;
}
