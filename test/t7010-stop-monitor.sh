#! /bin/sh -e

# Test to make sure that if the monitor is stopped and re-started, we don't
# hose up existing flags.

. ../tup.sh
tup monitor

echo "int main(void) {return 0;}" > foo.c
cp ../testTupfile.tup Tupfile
tup stop
tup monitor
update
tup_object_exist . foo.c foo.o prog
sym_check foo.o main
sym_check prog main

touch foo.c
tup stop
rm foo.o
tup monitor
update
tup_object_exist . foo.c foo.o prog
sym_check foo.o main
sym_check prog main
