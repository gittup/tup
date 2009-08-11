#! /bin/sh -e

# Like t5045, but with a symlink.
. ../tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
: |> touch foo; ln -s foo bar |> foo
HERE
tup touch sub/Tupfile
update_fail

check_not_exist sub/bar

cat > sub/Tupfile << HERE
: |> touch foo |> foo
HERE
tup touch sub/Tupfile
update

check_exist sub/foo
check_not_exist sub/bar
