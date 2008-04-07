#! /bin/sh -e

. ../tup.sh
cp ../testMakefile Makefile

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Makefile
update
sym_check foo.o foo
sym_check prog foo

cat Makefile | sed 's/prog := prog/prog := newprog/' > tmpMakefile
mv tmpMakefile Makefile
tup touch Makefile
update

sym_check newprog foo
check_not_exist prog
tup_object_no_exist "gcc foo.o -o prog"
tup_object_no_exist prog
