#! /bin/sh -e

# Make sure that if a command creates a file and then segfaults, the outputs
# the outputs are still checked.
. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	FILE *f;
	int *x = 0;

	f = fopen("tmp.txt", "w");
	if(!f) {
		fprintf(stderr, "Unable to open tmp.txt file for write in t5067.\n");
		return 1;
	}
	*x = 5;
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> tup_t5067_segfault2
: tup_t5067_segfault2 |> ./%f |>
HERE
tup touch ok.c Tupfile
update_fail_msg "signal 11 (Segmentation fault)"

cat > Tupfile << HERE
HERE
tup touch Tupfile
update
check_not_exist tmp.txt tup_t5067_segfault2

eotup
