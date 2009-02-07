#! /bin/sh -e

# This was an error case I discovered when I tried to move sysvinit out of
# gittup and compile everything. For some reason a directory gets marked as
# both create and delete, but the updater still tries to open the directory and
# update it. Basically we just set up some directories (so things are in
# create), then move it to another directory (so it goes in delete).

. ../tup.sh
tup monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
tup flush
mv a b
update
tup_object_exist . b
tup_object_exist b a2
tup_object_exist b/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
tup_object_no_exist . a
tup_object_no_exist a a2
tup_object_no_exist a/a2 foo.c foo.o prog 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog'
