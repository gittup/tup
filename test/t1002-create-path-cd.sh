#! /bin/sh -e

. ../tup.sh
tmkdir foo
cd foo
tup touch bar.c
cd ..
tup_object_exist foo bar.c
