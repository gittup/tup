#! /bin/sh -e

. ../tup.sh
cp ../testMakefile Makefile

# Verify both files are compiled
echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog main bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup delete bar.c
tup touch realbar.c
update
check_not_exist bar.o
tup_object_no_exist bar.c bar.o
sym_check realbar.o bar1
sym_check prog main bar1
