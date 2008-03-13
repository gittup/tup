#! /bin/sh

. ../tup.sh

echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
tup upd
check_empty_tupdirs
sym_check foo.o foo1
sym_check bar.o bar1
sym_check prog_ foo1 bar1

# When bar.o is deleted, it should be re-generated
rm bar.o
tup delete bar.o
tup upd
check_empty_tupdirs
sym_check bar.o bar1

# Similar for prog_
rm prog_
tup delete prog_
tup upd
check_empty_tupdirs
sym_check prog_ foo1 bar1
