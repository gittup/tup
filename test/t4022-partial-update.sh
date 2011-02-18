#! /bin/sh -e

# Test out updating with a specific target.

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1

echo "int main2(void) {}" > foo.c
echo "void bar2(void) {}" > bar.c
tup touch bar.c foo.c
update_partial foo.o

# Only bar.o should have the new symbol
sym_check bar.o ^bar2
sym_check foo.o main2
sym_check prog main bar1

eotup
