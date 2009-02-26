#! /bin/sh -e

# Now try to include a Tupfile using a relative path. From there, also include
# another file relative to it (Tupfile.vars and Tupfile.ccargs are in the same
# directory).

. ../tup.sh
mkdir a
cat > a/Tupfile << HERE
include ../Tupfile.vars
: foreach *.c |> \$(CC) \$(CCARGS) |> %F.o
: *.o |> \$(CC) -o prog %f |> prog
HERE

cat > Tupfile.vars << HERE
CC = gcc
include Tupfile.ccargs
HERE

cat > Tupfile.ccargs << HERE
CCARGS := -c %f
CCARGS += -o %o
HERE

tup touch a/foo.c a/bar.c a/Tupfile Tupfile.vars Tupfile.ccargs
tup parse
tup_object_exist a foo.c bar.c
tup_object_exist a "gcc -c foo.c -o foo.o"
tup_object_exist a "gcc -c bar.c -o bar.o"
tup_object_exist a "gcc -o prog bar.o foo.o"
