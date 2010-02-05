#! /bin/sh -e

. ./tup.sh
tmkdir foo
tup touch foo/bar.c
tup_object_exist foo bar.c

eotup
