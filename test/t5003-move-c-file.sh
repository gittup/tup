#! /bin/sh -e

. ../tup.sh

# Verify both files are compiled
echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o foo1
sym_check bar.o bar1
sym_check prog_ foo1 bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup delete bar.c
tup create realbar.c
update
check_not_exist bar.o
tup_object_no_exist bar.c bar.o
sym_check realbar.o bar1
sym_check prog_ foo1 bar1
