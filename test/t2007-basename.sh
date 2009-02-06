#! /bin/sh -e

# Test the basename flags (%B and %b)

. ../tup.sh
cat > Tupfile << HERE
: superlongtest/ok |> echo hey %f %o |> %B.o
: foreach subdir/*.txt |> echo cp %f %o |> %b
: foreach subdir/*.c |> echo gcc -c %f -o %o |> %B.o
HERE
mkdir subdir
mkdir superlongtest
tup touch subdir/foo.c subdir/readme.txt Tupfile superlongtest/ok
update
tup_object_exist . foo.o readme.txt
tup_object_exist . "echo cp subdir/readme.txt readme.txt"
tup_object_exist . "echo gcc -c subdir/foo.c -o foo.o"
tup_object_exist . "echo hey superlongtest/ok ok.o"
