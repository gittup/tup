#! /bin/sh -e

. ../tup.sh
mkdir foo
cd foo
mkdir baz
cd baz
tup touch ../bar.c
cd ../..
tup_object_exist foo bar.c
