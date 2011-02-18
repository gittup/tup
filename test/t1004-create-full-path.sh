#! /bin/sh -e

. ./tup.sh
mkdir foo
tup touch foo `/bin/pwd`/foo/bar.c
tup_object_exist foo bar.c

eotup
