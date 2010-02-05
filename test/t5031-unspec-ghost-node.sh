#! /bin/sh -e

# Make sure things work when a command tries to read from a ghost node, and
# then later a command is created that writes to that node. We should get
# yelled at that the input was unspecified.

. ./tup.sh
cat > Tupfile << HERE
: |> (cat ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt

cat > Tupfile << HERE
: |> echo yo > %o |> ghost
: |> (cat ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> echo yo > %o |> ghost
: ghost |> (cat ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo yo | diff - output.txt

eotup
