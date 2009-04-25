#! /bin/sh -e

# See if we remove the command that created a ghost node, that the ghost node
# goes away.

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
tup_object_exist . ghost

rm -f Tupfile
tup delete Tupfile
update
tup_object_no_exist . ghost
