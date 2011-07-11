#! /bin/sh -e

# Create a symlink to nowhere and then remove it. The ghost should also go away.
. ./tup.sh
check_monitor_supported
tup monitor

ln -s ghost foo
tup_object_exist . foo
rm foo
tup_object_no_exist . ghost foo

eotup
