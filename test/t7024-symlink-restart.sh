#! /bin/sh -e

# Make sure a symlink doesn't do anything funky when the monitor is restarted
. ./tup.sh
tup monitor

# First set everything up and clear all flags
mkdir foo-x86
ln -s foo-x86 foo
update

# Now fake the mtime to be old, and update again to clear the flags (since the
# mtime is different, it will get put into modify)
stop_monitor
tup fake_mtime foo 5
tup monitor
stop_monitor
update

# Now the mtime should match the symlink. Start and stop the monitor again to
# see that no flags are set.
tup monitor
stop_monitor
check_empty_tupdirs

eotup
