#! /bin/sh -e

# Same as the previous test, only we try some variable assignments.

. ../tup.sh
cat > Tupfile << HERE
CC = gcc
CCARGS := -DFOO=1
CCARGS += -DBAR=1
CC = echo \$(CC)
: foreach *.c |> \$(CC) -c %f -o %o \$(CCARGS) |> %F.o
: *.o |> \$(CC) -o prog %f |> prog
HERE
tup touch foo.c bar.c Tupfile
tup parse
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "echo gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "echo gcc -o prog bar.o foo.o"
