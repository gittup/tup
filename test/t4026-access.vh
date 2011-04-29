#! /bin/sh -e

# Try to use the access() function. It should always count as a read (similar
# to a stat()).

. ./tup.sh

cat > foo.c << HERE
#include <unistd.h>

int main(void)
{
	if(access("access.txt", F_OK) != 0)
		return 1;
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ./foo |>
HERE
tup touch foo.c Tupfile access.txt
update

tup_dep_exist . access.txt . "./foo"

eotup
