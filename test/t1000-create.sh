#! /bin/sh -e

. ../tup.sh
touch foo.c
tup_object_exist foo.c
tup_create_exist .
