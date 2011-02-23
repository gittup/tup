#! /bin/sh -e

. ./tup.sh
check_no_windows paths
mkdir foo
tup touch foo `/bin/pwd`/foo/bar.c
tup_object_exist foo bar.c

eotup
