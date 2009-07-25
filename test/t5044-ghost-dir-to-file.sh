#! /bin/sh -e

# Make sure changing a ghost dir to a real file works. Basically the same as
# t5044, only we're checking to see that a file can still have ghost children.

. ../tup.sh
cat > Tupfile << HERE
: |> (cat secret/ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt


touch secret
tup touch secret
update

rm secret
tup rm secret
tmkdir secret
echo boo > secret/ghost
tup touch secret/ghost
update

echo boo | diff - output.txt
