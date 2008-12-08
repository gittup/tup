#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

mkdir bar
cp ../testTupfile bar/Tupfile
echo "#define FOO 3" > foo.h
(echo "#include \"../foo.h\""; echo "int main(void) {return FOO;}") > bar/foo.c
tup touch foo.h
tup touch bar/foo.c
update
sym_check bar/foo.o main
tup_dep_exist . foo.h bar "gcc -c foo.c -o foo.o"
tup_dep_exist bar "gcc -c foo.c -o foo.o" bar foo.o
