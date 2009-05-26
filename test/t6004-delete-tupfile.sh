#! /bin/sh -e

. ../tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o foo
sym_check prog foo
tup_object_exist . "gcc -c foo.c -o foo.o"
tup_object_exist . "gcc foo.o -o prog"
tup_object_exist . foo.o
tup_object_exist . prog

rm Tupfile
tup rm Tupfile
update

check_not_exist foo.o prog
tup_object_no_exist . "gcc -c foo.c -o foo.o"
tup_object_no_exist . "gcc foo.o -o prog"
tup_object_no_exist . foo.o
tup_object_no_exist . prog
