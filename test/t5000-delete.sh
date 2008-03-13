#! /bin/sh

. ../tup.sh

# Verify both files are compiled
echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
echo "void baz1(void) {}" > baz.c
tup touch foo.c bar.c baz.c
tup upd
check_empty_tupdirs
object_check foo.o foo1
object_check bar.o bar1
object_check baz.o baz1
object_check prog_ foo1 bar1 baz1

rm baz.c
tup delete baz.c
tup upd
check_empty_tupdirs
check_not_exist baz.o
object_check prog_ foo1 bar1 ~baz1

tup_object_exist foo.c foo.o bar.c bar.o prog_
tup_object_no_exist baz.c baz.o
