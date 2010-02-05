#! /bin/sh -e

. ./tup.sh
mkdir foo
tup touch foo $PWD/foo/bar.c
tup_object_exist foo bar.c

eotup
