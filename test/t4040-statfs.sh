#! /bin/sh -e

# Try statfs

. ./tup.sh

cat > ok.c << HERE
#ifdef __APPLE__
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif

int main(void)
{
	struct statfs buf;
	if(statfs("foo.txt", &buf) < 0) {
		perror("statfs");
		return 1;
	}
	return 0;
}
HERE

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok
: ok |> ./%f |>
HERE
tup touch ok.c foo.txt Tupfile
update

tup_dep_exist . foo.txt . ./ok

eotup
