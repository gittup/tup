#! /bin/sh -e

. ./tup.sh
tmkdir foo
cd foo
tmkdir baz
cd baz
tup touch ../bar.c
cd ../..
tup_object_exist foo bar.c

eotup
