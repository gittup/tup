#! /bin/sh -e

. ../tup.sh
tup touch foo/bar.c
tup_object_exist foo bar.c
