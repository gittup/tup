#! /bin/sh -e

# In between updates, we should be able to have a node go from a directory to
# a file and back again.

. ./tup.sh

tmkdir atmp
update

rmdir atmp
touch atmp
update

rm atmp
mkdir atmp
update

eotup
