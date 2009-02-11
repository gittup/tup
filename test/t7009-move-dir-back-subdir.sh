#! /bin/sh -e

# Same as t7008, only we move the dir back to its original spot before doing
# the update.

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

# There and back again.
mv a b
tup flush
mv b a
update
tup_object_exist . a
tup_object_exist a a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_exist a/a2 foo.c
sym_check a/foo.o main

tup_object_no_exist . b
tup_object_no_exist b a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_no_exist b/a2 foo.c
