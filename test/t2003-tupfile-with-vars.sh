#! /bin/sh -e

# Same as the previous test, only we try some variable assignments.

. ../tup.sh
cat > Tupfile << HERE
CC = gcc
CCARGS := -c %f
CCARGS += -o %F.o
CC = echo \$(CC)
: foreach *.c >> \$(CC) \$(CCARGS) >> %F.o
: *.o >> \$(CC) -o prog %f >> prog
HERE
tup touch foo.c bar.c Tupfile
tup upd
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o"
tup_object_exist . "echo gcc -c bar.c -o bar.o"
tup_object_exist . "echo gcc -o prog foo.o bar.o"
