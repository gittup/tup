#! /bin/sh -e

# Test the basename flags (%B and %b)

. ../tup.sh
cat > Tupfile << HERE
: superlongtest/ok |> cp %f %o |> %B.o
: foreach subdir/*.txt |> cp %f %o |> %b
: foreach subdir/*.c |> gcc -c %f -o %o |> %B.o
HERE
tmkdir subdir
tmkdir superlongtest
tup touch subdir/foo.c subdir/readme.txt Tupfile superlongtest/ok
tup parse
tup_object_exist . foo.o readme.txt
tup_object_exist . "cp subdir/readme.txt readme.txt"
tup_object_exist . "gcc -c subdir/foo.c -o foo.o"
tup_object_exist . "cp superlongtest/ok ok.o"
