#! /bin/sh -e

. ../tup.sh
cp ../testMakefile Makefile

(echo "#include \"foo.h\""; echo "int main(void) {}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "int x;" > foo.h
tup touch foo.c bar.c foo.h
update
sym_check foo.o main x
sym_check bar.o bar1 x
sym_check prog main bar1

# If we re-compile bar.c without the header, foo.h will have a dangling ref
# to bar.o
echo "void bar1(void) {}" > bar.c
tup touch bar.c
update
sym_check bar.o bar1 ~x

# Now the tricky part - we touch foo.h (which has refs to both .o files) and
# see if only foo.c is re-compiled. Also, the foo.h->bar.o link should be gone
rm foo.o
ln bar.o oldbar.o
tup touch foo.h
update
check_same_link bar.o oldbar.o
rm oldbar.o
sym_check foo.o main x
tup_dep_no_exist foo.h bar.o

# Make sure the foo.h->cmd link still exists and wasn't marked obsolete for
# some reason.
tup touch foo.h
update
tup_dep_exist foo.h "tup wrap gcc -c foo.c -o foo.o"
