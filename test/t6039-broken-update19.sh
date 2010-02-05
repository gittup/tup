#! /bin/sh -e

# Apparently I broke Tuprules.tup with @-variables when I removed the
# tup_entry_add's a commit or two ago. Only happens when the monitor is
# running.

. ./tup.sh

tup monitor
cat > Tuprules.tup << HERE
files-@(ARCH) = foo.c
HERE
cat > Tupfile << HERE
include_rules
HERE
varsetall ARCH=y
tup touch Tuprules.tup Tupfile
update

tup touch Tupfile
update

eotup
