#! /bin/sh

. ../tup.sh

echo "void foo1(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o foo1

echo "void foo2(void) {}" >> foo.c
tup touch foo.c
update
sym_check foo.o foo1 foo2
