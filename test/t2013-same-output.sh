#! /bin/sh -e

# Verify that if two commands output the same file, it explodes.

. ../tup.sh
cat > Tupfile << HERE
: foreach *.c |> echo gcc -c %f -o %o |> foo.o
HERE
tup touch foo.c bar.c Tupfile
update_fail
