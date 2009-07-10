#! /bin/sh -e

# Like t5038, but with multiple wrong files.
. ../tup.sh

cat > Tupfile << HERE
: |> touch foo; touch bar; touch baz |> foo
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
