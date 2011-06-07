#! /bin/sh -e

# Try to use the access() function after opening a file for write.

. ./tup.sh
check_no_windows shell

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
: foo |> touch %o && ./foo |> access.txt
HERE
tup touch foo.c Tupfile
update

eotup
