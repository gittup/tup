#! /bin/sh -e

# Try truncate.

. ./tup.sh

cat > foo.c << HERE
#include <unistd.h>
#include <sys/types.h>

int main(void)
{
	if(truncate("tmp.txt", 1) < 0) {
		perror("tmp.txt");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> echo heythere > %o; ./foo |> tmp.txt
HERE
tup touch Tupfile foo.c
update

echo -n 'h' | diff - tmp.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
HERE
tup touch Tupfile
update
check_not_exist tmp.txt

cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ./foo |>
HERE
tup touch Tupfile tmp.txt
update_fail_msg "tup error.*truncate"

eotup
