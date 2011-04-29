#! /bin/sh -e

# Try to use readlink, both on a regular link and one that we create during
# the job.

. ./tup.sh

cat > foo.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char buf[128];
	int size;

	size = readlink("slink.txt", buf, sizeof(buf));
	if(size < 0) {
		perror("readlink");
		return 1;
	}
	if(strncmp(buf, "target.txt", 10) != 0) {
		printf("readlink doesn't match target :(\n");
		return 1;
	}
	return 0;
}
HERE
tup touch target.txt
ln -s target.txt slink.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ./foo |>
HERE
tup touch slink.txt Tupfile
update

rm slink.txt
tup rm slink.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ln -s target.txt slink.txt && ./foo |> slink.txt
HERE
tup touch Tupfile
update

eotup
