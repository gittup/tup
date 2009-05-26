#! /bin/sh -e

# Make a symlink to a ghost file in a ghost directory.
. ../tup.sh
ln -s spooky/ghost foo
tup touch foo
tup_object_exist . spooky foo
tup_object_exist spooky ghost

rm -f foo
tup rm foo
tup_object_no_exist . foo
tup_object_no_exist spooky ghost
tup_object_no_exist . spooky
