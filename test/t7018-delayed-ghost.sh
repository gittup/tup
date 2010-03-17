#! /bin/sh -e

. ./tup.sh
check_monitor_supported

# Check to see if we can create a ghost node, then shutdown the monitor and
# turn the ghost into a file.
tup monitor
cat > Tupfile << HERE
: |> if [ -f ghost ]; then cat ghost; else echo nofile; fi > %o |> output.txt
HERE
update
stop_monitor
echo nofile | diff - output.txt
tup_object_exist . ghost

echo foo > ghost
tup monitor
update
stop_monitor
echo foo | diff - output.txt

eotup
