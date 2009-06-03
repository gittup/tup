#! /bin/sh -e

# Try order-only-ish prerequisites. Now with bins! (copied from t2011)

. ../tup.sh
cat > Tupfile << HERE
: |> echo blah > %o |> foo.h {headers}
: foreach *.c | {headers} |> echo gcc -c %f -o %o |> %F.o
HERE

tup touch Tupfile foo.c bar.c
tup parse
tup_dep_exist . foo.h . "echo gcc -c foo.c -o foo.o"
tup_dep_exist . foo.h . "echo gcc -c bar.c -o bar.o"
