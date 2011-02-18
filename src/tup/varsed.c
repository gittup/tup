#include "varsed.h"
#include "vardict.h"
#include "fslurp.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static int var_replace(int ifd, int ofd, int binmode);

int varsed(int argc, char **argv)
{
	int ifd = 0;
	int ofd = 1;
	int x;
	int binmode = 0;
	int input_found = 0;

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-h") == 0 ||
		   strcmp(argv[x], "--help") == 0) {
			fprintf(stderr, "Usage: %s [infile] [outfile]\n", argv[0]);
			fprintf(stderr, "This will replace all variable references of the form @VARIABLE@ in [infile] with the corresponding value in the tup database, and write the output to [outfile]. If not specified, or specified as \"-\", then the input and output default to stdin and stdout, respectively.\n");
			return 1;
		} else if(strcmp(argv[x], "--binary") == 0) {
			binmode = 1;
		} else {
			if(!input_found) {
				if(strcmp(argv[x], "-") != 0) {
					ifd = open(argv[x], O_RDONLY);
					if(ifd < 0) {
						fprintf(stderr, "Error opening input file.\n");
						perror(argv[x]);
						return 1;
					}
				}
				input_found = 1;
			} else {
				if(strcmp(argv[x], "-") != 0) {
					ofd = creat(argv[x], 0666);
					if(ofd < 0) {
						fprintf(stderr, "Error creating output file.\n");
						perror(argv[x]);
						return 1;
					}
				}
			}
		}
	}

	if(tup_vardict_init() < 0)
		return 1;

	if(var_replace(ifd, ofd, binmode) < 0)
		return 1;
	return 0;
}

static int var_replace(int ifd, int ofd, int binmode)
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
				if(binmode && len == 1) {
					if(value[0] == 'y')
						value = "1";
					else if(value[0] == 'n')
						value = "0";
				}
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
