#! /bin/sh -e

# Kinda like t7009, only we don't mess with the directory that has the Tupfile.

. ./tup.sh
check_monitor_supported
tup monitor
mkdir a
mkdir output

cat > output/Tupfile << HERE
: foreach ../a/*.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > a/foo.c
update
tup_object_exist . a
tup_object_exist a foo.c
tup_object_exist output foo.o 'gcc -c ../a/foo.c -o foo.o'
sym_check output/foo.o main

# There and back again.
mv a b
tup flush
mv b a
update
tup_object_exist . a
tup_object_exist a foo.c
tup_object_exist output foo.o 'gcc -c ../a/foo.c -o foo.o'
sym_check output/foo.o main

tup_object_no_exist . b
tup_object_no_exist b foo.c

eotup
