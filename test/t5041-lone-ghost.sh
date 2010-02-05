#! /bin/sh -e

# Make sure a lone ghost in a directory doesn't end up killing the directory
# that it's in.

. ./tup.sh
tmkdir foo
cat > Tupfile << HERE
: |> (cat foo/ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt

cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_exist . foo

eotup
