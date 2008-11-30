#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main

echo "void foo2(void) {}" >> foo.c
tup touch foo.c
update
sym_check foo.o main foo2
