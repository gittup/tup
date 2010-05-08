#include <windows.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char* argv[])
{
	int i;

	if (argc < 3) {
		fprintf(stderr, "tup-cp [sources...] [destination]\n");
		exit(-1);
	}

	for (i = 1; i < argc - 1; i++) {
		if (!CopyFileA(argv[i], argv[argc-1], FALSE)) {
			perror("copy");
		}
	}

	return 0;
}
