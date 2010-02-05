#! /bin/sh -e

# Make sure we can't write to a file in the wrong directory using %f

. ./tup.sh
tmkdir sub
touch sub/foo.c
cat > Tupfile << HERE
: sub/foo.c |> gcc -c %f -o %o |> %f.o
HERE
tup touch sub/foo.c Tupfile
update_fail
check_not_exist foo.o sub/foo.o

eotup
