#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o foo
sym_check prog foo

cat Tupfile | sed 's/prog/newprog/g' > tmpTupfile
mv tmpTupfile Tupfile
tup touch Tupfile
update

sym_check newprog foo
check_not_exist prog
tup_object_no_exist "gcc foo.o -o prog"
tup_object_no_exist prog
