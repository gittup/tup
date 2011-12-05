#! /bin/sh -e

# Make sure just changing to a new directory doesn't update the environment.

. ./tup.sh

cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o
HERE

tmkdir sub

tup touch foo.c Tupfile
update

check_exist foo.o
rm foo.o
cd sub
update --no-scan
cd ..

check_not_exist foo.o

eotup
