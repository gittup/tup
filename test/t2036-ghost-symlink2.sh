#! /bin/sh -e

# See if we make a symlink to a real file, that the real file isn't removed
# just because the symlink was.

. ./tup.sh
tup touch real
ln -s real foo
tup touch foo
tup_object_exist . real foo

rm -f foo
tup rm foo
tup_object_no_exist . foo
tup_object_exist . real

eotup
