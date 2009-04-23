#! /bin/sh -e

# Similar to t2028, only the ghost file is now created by a new rule.

. ../tup.sh
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
echo nofile | diff output.txt -

cat > Tupfile << HERE
: |> echo alive > %o |> ghost
: ghost |> ./ok.sh > %o |> output.txt
HERE

tup touch Tupfile
update
echo alive | diff output.txt -
