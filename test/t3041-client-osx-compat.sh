#! /bin/sh -e

# OSX tries to call access() on the @tup@ directory before calling getattr()
# on the @tup@/FOO variable. This test makes sure that access() on @tup@
# will succeed to support that.

. ./tup.sh
check_no_windows varsed

cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	if(access("@tup@", R_OK) < 0) {
		perror("access @tup@");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok
: ok |> ./ok |>
HERE
tup touch ok.c Tupfile
update

eotup
