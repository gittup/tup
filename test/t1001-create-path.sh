#! /bin/sh -e

. ../tup.sh
mkdir foo
tup touch foo/bar.c
tup_object_exist foo bar.c
