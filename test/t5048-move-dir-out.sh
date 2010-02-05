#! /bin/sh -e

# Try to move a directory out of tup, and make sure dependent files are still
# compiled.

. ./tup.sh
mkdir real
cd real
re_init
tmkdir sub
echo "#define FOO 3" > sub/foo.h
echo '#include "foo.h"' > foo.c
echo ': foreach *.c |> gcc -c %f -o %o -Isub |> %B.o' > Tupfile
tup touch Tupfile foo.c sub/foo.h
update

mv sub ..
tup rm sub
update_fail

tup touch foo.h
update

eotup
