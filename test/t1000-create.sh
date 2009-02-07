#! /bin/sh -e

. ../tup.sh
tup touch foo.c
tup_object_exist . foo.c
