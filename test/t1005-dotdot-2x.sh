#! /bin/sh -e

# Make sure ../.. doesn't get condensed to nothing.

. ../tup.sh
tmkdir foo
cd foo
tmkdir baz
cd baz
tup touch ../../bar.c
tup touch ../baz/../../foo.c
cd ../..
tup_object_exist . bar.c
tup_object_exist . foo.c
