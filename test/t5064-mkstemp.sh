#! /bin/sh -e

# Check support for mkstemp

. ./tup.sh
check_no_windows mkstemp
cat > ok.c << HERE
#include <stdlib.h>

int main(void)
{
	char template[] = "output.XXXXXX";
	int fd;

	fd = mkstemp(template);
	if(fd < 0) {
		perror("mkstemp");
		return 1;
	}
	write(fd, "text", 4);
	close(fd);
	rename(template, "output");
	return 0;
}
HERE

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> prog
: prog |> ./prog |> output
HERE
tup touch ok.c Tupfile
update

tup_dep_exist . './prog' . output

eotup
