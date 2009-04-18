#! /bin/sh -e

# Now try to include a Tupfile using a relative path. From there, also include
# another file relative to it (Tupfile.vars and Tupfile.ccargs are in the same
# directory).

. ../tup.sh
tmkdir a
cat > a/Tupfile << HERE
include ../Tupfile.vars
: foreach *.c |> \$(CC) -c %f -o %o \$(CCARGS) |> %F.o
: *.o |> \$(CC) -o prog %f |> prog
HERE

cat > Tupfile.vars << HERE
CC = gcc
include Tupfile.ccargs
HERE

cat > Tupfile.ccargs << HERE
CCARGS := -DFOO=1
CCARGS += -DBAR=1
HERE

tup touch a/foo.c a/bar.c a/Tupfile Tupfile.vars Tupfile.ccargs
tup parse
tup_object_exist a foo.c bar.c
tup_object_exist a "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist a "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist a "gcc -o prog bar.o foo.o"
