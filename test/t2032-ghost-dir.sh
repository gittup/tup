#! /bin/sh -e

# See what happens if we try to read from a non-existent file in a non-existent
# directory.

. ../tup.sh
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
echo nofile | diff output.txt -

tmkdir secret
echo 'alive' > secret/ghost
tup touch secret/ghost
update
echo alive | diff output.txt -
