#! /bin/sh -e

# Make sure 'tup upd foo.c; tup upd foo.o' will still update the object file.

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update

echo "int foo; int main(void) {}" > foo.c
tup touch foo.c
update_partial foo.c
update_partial foo.o

sym_check foo.o foo
sym_check prog ^foo

update
sym_check prog foo

eotup
