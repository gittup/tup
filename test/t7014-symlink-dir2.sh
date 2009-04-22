#! /bin/sh -e

# Make sure a symlink doesn't go into the modify list when the monitor starts.
. ../tup.sh
tup monitor

mkdir foo-x86
ln -s foo-x86 foo

update
tup stop

tup monitor
check_empty_tupdirs
