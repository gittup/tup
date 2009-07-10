#! /bin/sh -e

# Make sure writing to a file that tup doesn't know about at all is removed
# from the filesystem.
. ../tup.sh

cat > Tupfile << HERE
: |> touch foo; touch bar |> foo
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> touch foo |> foo
HERE
tup touch Tupfile
update

check_exist foo
check_not_exist bar
