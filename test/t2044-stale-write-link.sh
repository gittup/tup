#! /bin/sh -e

# Check to make sure that stale write links are properly removed.

. ../tup.sh
cat > Tupfile << HERE
: |> touch foo |> bar
HERE
tup touch Tupfile
tup parse
tup_dep_exist . 'touch foo' . bar

cat > Tupfile << HERE
: |> touch foo |> foo
: |> touch bar |> bar
HERE
tup touch Tupfile
tup parse
tup_dep_exist . 'touch foo' . foo
tup_dep_exist . 'touch bar' . bar
tup_dep_no_exist . 'touch foo' . bar
