#! /bin/sh -e

# Make sure autoupdate works when the monitor has fchdir'd down a few
# directories.

. ./tup.sh
tup monitor
tup config autoupdate 1
cat > foo.c << HERE
int main(void) {return 3;}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> prog
HERE

tup flush
check_exist prog

mkdir foo
cp foo.c foo
cp Tupfile foo

tup flush
check_exist foo/prog

mkdir foo/bar
cp foo.c foo/bar
cp Tupfile foo/bar

tup flush
check_exist foo/bar/prog

eotup
