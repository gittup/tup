#! /bin/sh -e

# See if we correctly get updated if we try to use a file that doesn't exist,
# which is later created.

. ./tup.sh
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

echo 'alive' > ghost
tup touch ghost
update
echo alive | diff output.txt -

eotup
