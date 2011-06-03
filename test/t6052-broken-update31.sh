#! /bin/sh -e

# Make sure we can use 'touch -h' on a generated symlink correctly.

. ./tup.sh

touch foo
cat > Tupfile << HERE
: |> ln -s foo sym; touch -h sym |> sym
HERE
tup touch foo Tupfile
update

eotup
