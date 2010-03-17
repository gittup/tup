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

# Now we make another change outside of the monitor's control (modify a file).
# We have to fudge the timestamp to be in the future because we just stopped
# the monitor above (so the monitor has the current time). When we change
# bar.c, it will also have the current time, so it will still seem to be up-to-
# date. Ordinarily this wouldn't happen to a user since they aren't so fast.
# And it only happens in computer world if you: 1) run monitor, 2) cause the
# monitor to save its timestamp (by touching a file, or running the updater),
# 3) stop the monitor, 4) change a file, and then 5) start the monitor again,
# all without time(NULL) changing (ie: within a second). It just so happens
# that we do that exact sequence here.
#
# Also note that this test case will break in the year 2020, since I can't
# figure out how to just do time(now) + 1 with the shell. Assuming the
# Decepticons don't take over by then, I leave this message for my future self:
#
# Happy Birthday, Future Self! Hope you like fixing test cases.
#  - Self, 2009
echo "void bar2(void) {}" >> bar.c
touch -t 202005080000 bar.c
tup monitor
update
sym_check prog bar bar2
stop_monitor

# Finally, delete a file outside of the monitor's control
rm bar.c
tup monitor
update
sym_check prog ~bar ~bar2
stop_monitor

eotup
