#! /bin/sh -e

# Apparently a symlink to '.' causes tup to segfault.

. ./tup.sh

tmkdir sub
cd sub
ln -s . foo
update

eotup
