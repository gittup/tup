#! /bin/sh -e

. ./tup.sh
check_monitor_supported
tup monitor
mkdir a
cp ../testTupfile.tup a/Tupfile

echo "int main(void) {return 0;}" > a/foo.c
update
tup_object_exist . a
tup_object_exist a foo.c foo.o prog
sym_check a/foo.o main
sym_check a/prog main

# Move directory a to b
mv a b
update
tup_object_exist . b
tup_object_exist b foo.c foo.o prog
tup_object_no_exist . a
tup_object_no_exist a foo.c foo.o prog

eotup
