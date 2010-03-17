#! /bin/sh -e

# We have a ghost node, and then move a directory over that node. Since the
# directory node just gets renamed, we have to make sure the ghost becomes a
# normal node.
. ./tup.sh
check_monitor_supported
tup monitor
mkdir a
mkdir a/a2
echo 'heyo' > a/ghost
echo ': |> if [ -f b/ghost ]; then cat b/ghost; else echo nofile; fi > %o |> output' > Tupfile

update
echo 'nofile' | diff - output

mv a b
update
echo 'heyo' | diff - output

eotup
