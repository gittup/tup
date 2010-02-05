#! /bin/sh -e

# Test bang macros.

. ./tup.sh
cat > Tupfile << HERE
headers = foo.h
!cc = | foo.h |> gcc -c %f -o %o |>
: foo.h.in |> cp %f %o |> foo.h
: foreach *.c |> !cc |> %B.o
HERE
echo '#define FOO 3' > foo.h.in
echo '#include "foo.h"' > foo.c
tup touch foo.h.in foo.c bar.c Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.h . 'gcc -c bar.c -o bar.o'

check_updates foo.h.in foo.o
check_no_updates foo.h.in bar.o

eotup
