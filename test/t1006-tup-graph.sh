#! /bin/sh -e

# Make sure 'tup graph .' doesn't crash, since it has a slightly different code
# path that is not normally exercised.
. ./tup.sh
tup touch foo.c
tup graph . > /dev/null

eotup
