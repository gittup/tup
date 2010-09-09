#! /bin/sh -e

# Create a directory and then immediately remove it. The monitor should still
# function.

. ./tup.sh
check_monitor_supported
tup monitor

mkdir foo; rmdir foo
tup_object_no_exist . foo

mkdir foo
tup_object_exist . foo

stop_monitor

eotup
