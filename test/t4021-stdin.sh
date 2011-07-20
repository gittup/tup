#! /bin/sh -e

# Make sure a command that tries to read from stdin won't hang tup.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int i;
	char c = 0;
	i = read(0, &c, 1);
	if(i != 0) {
		fprintf(stderr, "Error: read test should return 0\n");
		return 1;
	}
	if(c != 0) {
		fprintf(stderr, "Error: char 'c' should not be set.\n");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> read_test
: read_test |> ./%f |>
HERE
update

eotup
