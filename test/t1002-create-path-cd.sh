#! /bin/sh -e

. ../tup.sh
mkdir foo
cd foo
tup touch bar.c
cd ..
tup_object_exist foo/bar.c
