#! /bin/sh -e

. ./tup.sh
check_monitor_supported

# First create a foo.c program, then stop the monitor
tup monitor
cp ../testTupfile.tup Tupfile
echo "int main(void) {return 0;}" > foo.c
update
stop_monitor

# Now we make a change outside of the monitor's control (create a new file)
echo "void bar(void) {}" > bar.c
tup monitor
update
tup_object_exist . bar.c bar.o
sym_check prog bar
stop_monitor

sleep 1
echo "void bar2(void) {}" >> bar.c
tup monitor
update
sym_check prog bar bar2
stop_monitor

# Finally, delete a file outside of the monitor's control
rm bar.c
tup monitor
update
sym_check prog ^bar ^bar2
stop_monitor

eotup
