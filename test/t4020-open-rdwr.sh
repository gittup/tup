#! /bin/sh -e

# Make sure open with O_RDWR works properly

. ./tup.sh

cat > prog.c << HERE
#include <stdio.h>
#include <fcntl.h>

int main(void)
{
	close(open("output", O_RDWR|O_CREAT, 0666));
	return 0;
}
HERE
gcc prog.c -o prog

cat > Tupfile << HERE
: |> ./prog |> output
HERE
tup touch prog Tupfile
update

tup_dep_exist . './prog' . output

eotup
