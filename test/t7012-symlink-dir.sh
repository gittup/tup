#! /bin/sh -e

# Make sure we can use a symlink from the monitor
. ./tup.sh
tup monitor

mkdir foo-x86
echo "#define PROCESSOR 86" > foo-x86/processor.h
ln -s foo-x86 foo

cat > foo.c << HERE
#include "foo/processor.h"
int foo(void) {return PROCESSOR;}
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
update

tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist foo-x86 processor.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . 'gcc -c foo.c -o foo.o' . foo.o

eotup
