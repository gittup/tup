#! /bin/sh -e

# Same as t2044, only the order of the rules in the second Tupfile are
# reversed.

. ../tup.sh
cat > Tupfile << HERE
: |> touch foo |> bar
HERE
tup touch Tupfile
tup parse
tup_dep_exist . 'touch foo' . bar

cat > Tupfile << HERE
: |> touch bar |> bar
: |> touch foo |> foo
HERE
tup touch Tupfile
tup parse
tup_dep_exist . 'touch foo' . foo
tup_dep_exist . 'touch bar' . bar
tup_dep_no_exist . 'touch foo' . bar
