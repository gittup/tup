#! /bin/sh -e

# Test to make sure that if the monitor is stopped and re-started, we don't
# hose up existing flags. Now with an included Tupfile

. ../tup.sh
tup monitor

touch foo
echo "include foo" > Tupfile
update
tup stop
tup_object_exist . foo Tupfile
tup_dep_exist . foo 0 .

# If we just stop and then start the monitor after an update, no flags should
# be set.
tup monitor
check_empty_tupdirs
