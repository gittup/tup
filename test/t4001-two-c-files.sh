#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

# Verify both files are compiled
echo "int main(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1

# Verify only foo is compiled if foo.c is touched
echo "void foo2(void) {}" >> foo.c
tup touch foo.c
if tup upd | grep 'gcc -c' | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Only foo.c should have been compiled." 1>&2
	exit 1
fi
sym_check foo.o main foo2

# Verify both are compiled if both are touched, but only linked once
rm foo.o
tup touch foo.c bar.c
if tup upd | grep 'gcc bar.o foo.o -o prog' | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Program should have only been linked once." 1>&2
	exit 1
fi
check_empty_tupdirs
sym_check foo.o main foo2
sym_check bar.o bar1
