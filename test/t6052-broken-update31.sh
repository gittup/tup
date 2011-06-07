#! /bin/sh -e

# Make sure we can use 'touch -h' on a generated symlink correctly.

. ./tup.sh

touch foo

# Some machines may not have 'touch -h' - in which case just skip
# this test.
touch -h foo || eotup

cat > Tupfile << HERE
: |> ln -s foo sym; touch -h sym |> sym
HERE
tup touch foo Tupfile
update

eotup
