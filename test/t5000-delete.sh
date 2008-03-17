#! /bin/sh -e

. ../tup.sh

# Verify all files are compiled
echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
echo "void baz1(void) {}" > baz.c
tup touch foo.c bar.c baz.c
update
sym_check foo.o foo1
sym_check bar.o bar1
sym_check baz.o baz1
sym_check prog_ foo1 bar1 baz1

# When baz.c is deleted, baz.o should be deleted as well, and prog_ should be
# re-linked. The baz.[co] objects should be removed from .tup
rm baz.c
tup delete baz.c
update
check_not_exist baz.o
sym_check prog_ foo1 bar1 ~baz1

tup_object_exist foo.c foo.o bar.c bar.o prog_
tup_object_no_exist baz.c baz.o

rm foo.c bar.c
tup delete foo.c bar.c
update
check_not_exist foo.o bar.o prog_
tup_object_no_exist foo.c foo.o bar.c bar.o prog_
