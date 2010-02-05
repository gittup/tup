#! /bin/sh -e

# Make sure that if we have two symlinks pointing to the same ghost, deleting
# one symlink doesn't kill the ghost.

. ./tup.sh
ln -s ghost foo
ln -s ghost bar
tup touch foo bar
tup_object_exist . ghost foo bar

rm -f foo
tup rm foo
tup_object_no_exist . foo
tup_object_exist . ghost bar

rm -f bar
tup rm bar
tup_object_no_exist . ghost foo bar

eotup
