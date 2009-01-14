#! /bin/sh -e

# Now try to include the variables from another Tupfile

. ../tup.sh
cat > Tupfile << HERE
include Tupfile.vars
: foreach *.c |> \$(CC) \$(CCARGS) |> %F.o
: *.o |> \$(CC) -o prog %f |> prog
HERE

cat > Tupfile.vars << HERE
CC = gcc
CCARGS := -c %f
CCARGS += -o %o
CC = echo \$(CC)
HERE

tup touch foo.c bar.c Tupfile
tup upd
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o"
tup_object_exist . "echo gcc -c bar.c -o bar.o"
tup_object_exist . "echo gcc -o prog foo.o bar.o"
