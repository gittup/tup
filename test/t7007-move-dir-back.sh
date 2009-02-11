#! /bin/sh -e

# Try to move a directory, then move it back to the original directory.

. ../tup.sh
tup monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
update
tup_object_exist . a
tup_object_exist a a2
tup_object_exist a/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
sym_check a/a2/foo.o main
sym_check a/a2/prog main

# There and back again.
mv a b
tup flush
mv b a
update
tup_object_exist . a
tup_object_exist a a2
tup_object_exist a/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
sym_check a/a2/foo.o main
sym_check a/a2/prog main

tup_object_no_exist . b
tup_object_no_exist b a2
tup_object_no_exist b/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
