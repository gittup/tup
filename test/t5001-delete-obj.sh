#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog main bar1

# When bar.o is deleted, it should be re-generated
rm bar.o
tup delete bar.o
tup g
update
sym_check bar.o bar1

# Similar for prog
rm prog
tup delete prog
update
sym_check prog main bar1
