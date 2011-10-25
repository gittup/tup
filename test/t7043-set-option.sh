#! /bin/sh -e

# Make sure setting an option doesn't kill the monitor. The problem is vim
# will delete .options.swpx, which the monitor will see.

. ./tup.sh
check_monitor_supported
tup monitor

touch .tup/.options.swpx
tup flush
rm .tup/.options.swpx
tup stop

eotup
