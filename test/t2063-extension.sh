#! /bin/sh -e

# Test the %e extension variable, which is only valid in a foreach command.

. ../tup.sh
touch foo.c
touch bar.c
touch asm.S
cat > Tupfile << HERE
CFLAGS_S += -DASM
: foreach *.c *.S |> gcc \$(CFLAGS_%e) -c %f -o %o |> %F.o
HERE
tup touch foo.c bar.c asm.S Tupfile
update
tup_dep_exist . foo.c . 'gcc  -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc  -c bar.c -o bar.o'
tup_dep_exist . asm.S . 'gcc -DASM -c asm.S -o asm.o'
