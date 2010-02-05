#! /bin/sh -e

# Try order-only-ish prerequisites.

. ./tup.sh
cat > Tupfile << HERE
: |> echo blah > %o |> foo.h
: foreach *.c | foo.h |> echo gcc -c %f -o %o |> %B.o
HERE

tup touch Tupfile foo.c bar.c
tup parse
tup_dep_exist . foo.h . "echo gcc -c foo.c -o foo.o"
tup_dep_exist . foo.h . "echo gcc -c bar.c -o bar.o"

eotup
