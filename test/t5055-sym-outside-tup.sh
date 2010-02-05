#! /bin/sh -e

# Try to symlink to a file outside of tup.
. ./tup.sh

ln -s ./t5055-sym-outside-tup.sh foo
tup touch foo
update

eotup
