#! /bin/sh -e

. ../tup.sh

# Check to see if we can create a ghost node, then shutdown the monitor and
# turn the ghost into a symlink, then shutdown the monitor and change the
# symlink.
tup monitor
cat > Tupfile << HERE
: |> if [ -f ghost ]; then cat ghost; else echo nofile; fi > %o |> output.txt
HERE
update
tup stop
echo nofile | diff - output.txt
tup_object_exist . ghost

echo foo > bar
echo newfoo > baz
ln -s bar ghost
tup monitor
update
tup stop
echo foo | diff - output.txt

rm ghost
ln -s baz ghost
tup monitor
update
tup stop
echo newfoo | diff - output.txt
