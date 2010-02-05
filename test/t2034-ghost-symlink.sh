#! /bin/sh -e

# See if we make a ghost node because of a symlink that it gets deleted when
# the broken symlink is removed.

. ./tup.sh
ln -s ghost foo
tup touch foo
tup_object_exist . ghost foo

rm -f foo
tup rm foo
tup_object_no_exist . ghost foo

eotup
