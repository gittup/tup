#! /bin/sh -e

# Make sure the access() function works on a tmpdir. This mimics how OSX
# uses temporary directories.

. ./tup.sh

cat > ok.c << HERE
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(void)
{
	if(mkdir("tmpdir", 0777) < 0) {
		perror("mkdir");
		return 1;
	}
	if(access("tmpdir", R_OK) < 0) {
		perror("access");
		return 1;
	}
	rmdir("tmpdir");
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
