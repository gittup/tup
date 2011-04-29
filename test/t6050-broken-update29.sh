#! /bin/sh -e

# Make sure stale files in the tmp directory don't break tup.

. ./tup.sh

cat > Tupfile << HERE
: |> echo hey > %o |> out.txt
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> echo yo > %o |> out.txt
: |> echo hey > %o |> out2.txt
HERE
tup touch Tupfile
touch .tup/tmp/0
update

eotup
