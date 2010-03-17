#! /bin/sh -e

. ./tup.sh
check_monitor_supported

# Check to see if we can create a ghost node, then shutdown the monitor and
# turn the ghost into a symlink, then shutdown the monitor and change the
# symlink.
tup monitor
cat > Tupfile << HERE
: |> if [ -f ghost ]; then cat ghost; else echo nofile; fi > %o |> output.txt
HERE
update
stop_monitor
echo nofile | diff - output.txt
tup_object_exist . ghost

echo foo > bar
echo newfoo > baz
ln -s bar ghost
tup monitor
update
stop_monitor
echo foo | diff - output.txt

rm ghost
ln -s baz ghost
tup monitor
update
stop_monitor
echo newfoo | diff - output.txt

eotup
