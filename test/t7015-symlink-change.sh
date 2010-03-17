#! /bin/sh -e

# See what happens if we change where a symlink points while the monitor is away
. ./tup.sh
check_monitor_supported
tup monitor

mkdir foo-x86
mkdir foo-ppc
echo "#define PROCESSOR 86" > foo-x86/processor.h
echo "#define PROCESSOR 12" > foo-ppc/processor.h
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
tup_dep_no_exist foo-ppc processor.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . 'gcc -c foo.c -o foo.o' . foo.o

stop_monitor
rm foo
ln -s foo-ppc foo
tup monitor
update

tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist foo-x86 processor.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist foo-ppc processor.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . 'gcc -c foo.c -o foo.o' . foo.o

eotup
