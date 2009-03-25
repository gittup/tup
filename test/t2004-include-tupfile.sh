#! /bin/sh -e

# Now try to include the variables from another Tupfile

. ../tup.sh
cat > Tupfile << HERE
include Tupfile.vars
: foreach *.c |> \$(CC) -c %f -o %o \$(CCARGS) |> %F.o
: *.o |> \$(CC) -o prog %f |> prog
HERE

cat > Tupfile.vars << HERE
CC = gcc
CCARGS := -DFOO=1
CCARGS += -DBAR=1
CC = \$(CC)
HERE

echo "int main(void) {return 0;}" > foo.c
touch bar.c
tup touch foo.c bar.c Tupfile Tupfile.vars
update
tup_object_exist . foo.c bar.c
tup_object_exist . "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -o prog bar.o foo.o"

# Now change the compiler to 'cc' and verify that we re-parse the parent
# Tupfile to generate new commands and get rid of the old ones.
cat > Tupfile.vars << HERE
CC = cc
CCARGS := -DFOO=1
CCARGS += -DBAR=1
CC = \$(CC)
HERE
tup touch Tupfile.vars
update
tup_object_no_exist . "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_no_exist . "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_no_exist . "gcc -o prog bar.o foo.o"
tup_object_exist . "cc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "cc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "cc -o prog bar.o foo.o"
