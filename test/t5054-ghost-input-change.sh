#! /bin/sh -e

# See what happens if the command string doesn't change, but the input
# dependencies of the command do. The ghost node should still be removed.

. ./tup.sh
cat > Tupfile << HERE
: ok.sh |> sh %f > %o |> output
HERE
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
echo 'heyo' > foo.txt
tup touch foo.txt Tupfile ok.sh
update

tup_dep_exist . ghost . 'sh ok.sh > output'

echo 'nofile' | diff - output
cat > ok.sh << HERE
cat foo.txt
HERE
tup touch ok.sh
update

echo 'heyo' | diff - output
tup_dep_no_exist . ghost . 'sh ok.sh > output'
tup_object_no_exist . ghost

eotup
