#! /bin/sh -e

# Stop monitor test, now with .gitignore, which is handled kinda differently
# from other files.

. ../tup.sh
tup monitor

mkdir .git
echo ".gitignore" > Tupfile
update
stop_monitor
tup_object_exist . .gitignore Tupfile

# If we just stop and then start the monitor after an update, no flags should
# be set.
tup monitor
check_empty_tupdirs
tup_object_exist . .gitignore Tupfile
