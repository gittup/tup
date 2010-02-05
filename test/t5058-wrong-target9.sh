#! /bin/sh -e

# Like t5038, but the file is written in a different directory
. ./tup.sh

tmkdir sub
cat > Tupfile << HERE
: |> touch foo; touch sub/bar |> foo
HERE
tup touch bar Tupfile
update_fail

check_exist bar
check_not_exist sub/bar

cat > Tupfile << HERE
: |> touch foo |> foo
HERE
tup touch Tupfile
update

check_exist foo bar
check_not_exist sub/bar

eotup
