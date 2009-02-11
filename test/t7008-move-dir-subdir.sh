#! /bin/sh -e

# Check if we move a dir that depends on its subdir, that things work.

. ../tup.sh
tup monitor
mkdir a
mkdir a/a2

cat > a/Tupfile << HERE
: foreach a2/*.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > a/a2/foo.c
update

tup_object_exist . a
tup_object_exist a a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_exist a/a2 foo.c
sym_check a/foo.o main

mv a b
update
tup_object_no_exist . a
tup_object_no_exist a a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_no_exist a/a2 foo.c

tup_object_exist . b
tup_object_exist b a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_exist b/a2 foo.c
sym_check b/foo.o main
