#! /bin/sh -e

# Make sure if we rm the .tup/db file then the monitor shuts down.

. ./tup.sh
check_monitor_supported
tup monitor

rm .tup/db
if tup stop; then
	echo "Error: tup stop should have failed." 1>&2
	exit 1
fi

eotup
