#! /bin/sh -e

# Like t5039, but with symlinks
. ./tup.sh

cat > Tupfile << HERE
: |> touch foo; ln -s foo bar; ln -s foo baz |> foo
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> touch foo |> foo
HERE
tup touch Tupfile
update

check_exist foo
check_not_exist bar baz

eotup
