#! /bin/sh -e

. ../tup.sh
tup monitor
mkdir a
mkdir a/a2
cp ../testMakefile Makefile
cp ../testMakefile a/Makefile
cp ../testMakefile a/a2/Makefile

echo "int main(void) {return 0;}" > a/a2/foo.c
update
tup_object_exist a/a2/foo.c a/a2/foo.o a/a2/prog a a/a2
sym_check a/a2/foo.o main
sym_check a/a2/prog main

# Move directory a to b
mv a b
update
tup_object_exist b/a2/foo.c b/a2/foo.o b/a2/prog
tup_object_no_exist a/a2/foo.c a/a2/foo.o a/a2/prog a a/a2
