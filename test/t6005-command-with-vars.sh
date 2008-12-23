#! /bin/sh -e

# Test that a rule in a Tupfile can use variables

. ../tup.sh
cat > Tupfile << HERE
CC = gcc
CFILES = *.c
OFILES = *.o
EXE = prog

: foreach \$(CFILES) >> \$(CC) -c %f -o %o >> %F.o
: \$(OFILES) >> \$(CC) %f -o \$(EXE) >> \$(EXE)
HERE

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o foo
sym_check prog foo
tup_object_exist . "gcc foo.o -o prog"
tup_object_exist . "gcc -c foo.c -o foo.o"
tup_object_exist . prog foo.o
