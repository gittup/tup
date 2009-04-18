#! /bin/sh -e

. ../tup.sh
tmkdir a
tmkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
tup touch a/a2/foo.c a/a2/Tupfile
update
tup_object_exist . a
tup_object_exist a a2
tup_object_exist a/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
sym_check a/a2/foo.o main
sym_check a/a2/prog main

# Move directory a to b
mv a b
tup delete a
tup touch b b/a2 b/a2/foo.c b/a2/Tupfile
update
tup_object_exist . b
tup_object_exist b a2
tup_object_exist b/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
tup_object_no_exist . a
tup_object_no_exist a a2
tup_object_no_exist a/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
