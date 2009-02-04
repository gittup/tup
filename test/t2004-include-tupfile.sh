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

tup touch foo.c bar.c Tupfile Tupfile.vars
update
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o"
tup_object_exist . "echo gcc -c bar.c -o bar.o"
tup_object_exist . "echo gcc -o prog foo.o bar.o"

# Now change the compiler to 'cc' and verify that we re-parse the parent
# Tupfile to generate new commands and get rid of the old ones.
cat > Tupfile.vars << HERE
CC = cc
CCARGS := -c %f
CCARGS += -o %o
CC = echo \$(CC)
HERE
tup touch Tupfile.vars
update
tup_object_no_exist . "echo gcc -c foo.c -o foo.o"
tup_object_no_exist . "echo gcc -c bar.c -o bar.o"
tup_object_no_exist . "echo gcc -o prog foo.o bar.o"
tup_object_exist . "echo cc -c foo.c -o foo.o"
tup_object_exist . "echo cc -c bar.c -o bar.o"
tup_object_exist . "echo cc -o prog foo.o bar.o"
