#! /bin/sh -e

# Make sure changing a ghost node to a generated node works.

. ./tup.sh
cat > Tupfile << HERE
: |> (cat secret/ghost 2>/dev/null || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt

cat > Tupfile << HERE
: |> echo yo > %o |> secret
HERE
tup touch Tupfile
update

eotup
