#! /bin/sh -e

# Make sure we can get a ghost if a file is used as a directory (so we get
# ENOTDIR as the error code).

. ./tup.sh
cat > Tupfile << HERE
: |> (cat secret/ghost 2>/dev/null || echo nofile) > %o |> output.txt
HERE
tup touch secret Tupfile
update
echo nofile | diff - output.txt

rm secret
tup rm secret
tmkdir secret
echo 'boo' > secret/ghost
tup touch secret/ghost
update

echo boo | diff - output.txt

eotup
