#! /bin/sh -e

. ../tup.sh
tup monitor
mkdir a
mkdir a/a2
cp ../testTupfile Tupfile
cp ../testTupfile a/Tupfile
cp ../testTupfile a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
update
tup_object_exist a/a2/foo.c a/a2/foo.o a/a2/prog a a/a2
sym_check a/a2/foo.o main
sym_check a/a2/prog main

rm -rf a
update
tup_object_no_exist a/a2/foo.c a/a2/foo.o a/a2/prog a a/a2
