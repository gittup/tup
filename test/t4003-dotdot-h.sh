#! /bin/sh

. ../tup.sh

mkdir bar
echo "#define FOO 3" > foo.h
(echo "#include \"../foo.h\""; echo "int foo1(void) {return FOO;}") > bar/foo.c
tup touch foo.h
tup touch bar/foo.c
update
sym_check bar/foo.o foo1
tup_dep_exist foo.h bar/foo.o
