#include "tup/client/tup_config_vars.h"
#include "tup/fslurp.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static int var_replace(int ifd, int ofd);

int main(int argc, char **argv)
{
	int ifd = 0;
	int ofd = 1;

	if(argc >= 2) {
		if(strcmp(argv[1], "-h") == 0 ||
		   strcmp(argv[1], "--help") == 0) {
			fprintf(stderr, "Usage: %s [infile] [outfile]\n", argv[0]);
			fprintf(stderr, "This will replace all variable references of the form @VARIABLE@ in [infile] with the corresponding value in the tup database, and write the output to [outfile]. If not specified, or specified as \"-\", then the input and output default to stdin and stdout, respectively.\n");
			return 1;
		}
		if(strcmp(argv[1], "-") != 0) {
			ifd = open(argv[1], O_RDONLY);
			if(ifd < 0) {
				fprintf(stderr, "Error opening input file.\n");
				perror(argv[1]);
				return 1;
			}
		}
	}
	if(argc >= 3) {
		if(strcmp(argv[2], "-") != 0) {
			ofd = creat(argv[2], 0666);
			if(ofd < 0) {
				fprintf(stderr, "Error creating output file.\n");
				perror(argv[2]);
				return 1;
			}
		}
	}
	if(var_replace(ifd, ofd) < 0)
		return 1;
	return 0;
}

static int var_replace(int ifd, int ofd)
{
	struct buf b;
	char *p, *e;

	if(fslurp(ifd, &b) < 0)
		return -1;

	p = b.s;
	e = b.s + b.len;
	do {
		char *at;
		char *rat;
		at = p;
		while(at < e && *at != '@') {
			at++;
		}
		if(write(ofd, p, at-p) != at-p) {
			perror("write");
			return -1;
		}
		if(at >= e)
			break;

		p = at;
		rat = p+1;
		while(rat < e && (isalnum(*rat) || *rat == '_')) {
			rat++;
		}
		if(rat < e && *rat == '@') {
			const char *value;

			value = tup_config_var(p+1, rat-(p+1));
			if(value) {
				int len;
				len = strlen(value);
				if(write(ofd, value, len) != len) {
					perror("write");
					return -1;
				}
			}
			p = rat + 1;
		} else {
			if(write(ofd, p, rat-p) != rat-p) {
				perror("write");
				return -1;
			}
			p = rat;
		}
		
	} while(p < e);

	return 0;
}
