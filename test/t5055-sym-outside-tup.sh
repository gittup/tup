#! /bin/sh -e

# Try to symlink to a file outside of tup.
. ../tup.sh

ln -s ../tup.sh foo
tup touch foo
update
