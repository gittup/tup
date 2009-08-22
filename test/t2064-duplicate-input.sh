#! /bin/sh -e

# Duplicate inputs should be an error.

. ../tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} foo.o |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c Tupfile
update_fail
