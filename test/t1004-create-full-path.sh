#! /bin/sh -e

. ../tup.sh
mkdir foo
tup touch $PWD/foo/bar.c
tup_object_exist foo/bar.c
