#! /bin/sh -e

. ../tup.sh
mkdir blah
cp ../testTupfile blah/Tupfile

# Verify all files are compiled
echo "int main(void) {return 0;}" > blah/foo.c
echo "void bar1(void) {}" > blah/bar.c
echo "void baz1(void) {}" > blah/baz.c
tup touch blah/foo.c blah/bar.c blah/baz.c blah/Tupfile
update
sym_check blah/foo.o main
sym_check blah/bar.o bar1
sym_check blah/baz.o baz1
sym_check blah/prog main bar1 baz1

# When baz.c is deleted, baz.o should be deleted as well, and prog should be
# re-linked. The baz.[co] objects should be removed from .tup
rm blah/baz.c
tup delete blah/baz.c
update
check_not_exist blah/baz.o
sym_check blah/prog main bar1 ~baz1

tup_object_exist blah foo.c foo.o bar.c bar.o prog
tup_object_no_exist blah baz.c baz.o

rm blah/foo.c blah/bar.c
tup delete blah/foo.c blah/bar.c
update
check_not_exist blah/foo.o blah/bar.o blah/prog
tup_object_no_exist blah foo.c foo.o bar.c bar.o prog
