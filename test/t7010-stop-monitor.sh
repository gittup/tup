#! /bin/sh -e

# Test to make sure that if the monitor is stopped and re-started, we don't
# hose up existing flags.

. ./tup.sh
tup monitor

echo "int main(void) {return 0;}" > foo.c
cp ../testTupfile.tup Tupfile
stop_monitor
tup monitor
update
tup_object_exist . foo.c foo.o prog
sym_check foo.o main
sym_check prog main

# Set foo.c's modify flags, then secretly remove foo.o behind the monitor's
# back (so we can see it gets re-created). When the monitor starts again, it
# shouldn't clear foo.c's flags.
touch foo.c
stop_monitor
rm foo.o
tup monitor
update
tup_object_exist . foo.c foo.o prog
sym_check foo.o main
sym_check prog main

# If we just stop and then start the monitor after an update, no flags should
# be set.
stop_monitor
tup monitor
check_empty_tupdirs

eotup
