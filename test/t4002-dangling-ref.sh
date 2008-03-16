#! /bin/sh

. ../tup.sh

(echo "#include \"foo.h\""; echo "void foo1(void) {}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "int x;" > foo.h
tup touch foo.c bar.c foo.h
tup upd
check_empty_tupdirs
sym_check foo.o foo1 x
sym_check bar.o bar1 x
sym_check prog_ foo1 bar1

# If we re-compile bar.c without the header, foo.h will have a dangling ref
# to bar.o
echo "void bar1(void) {}" > bar.c
tup touch bar.c
tup upd
check_empty_tupdirs
sym_check bar.o bar1 ~x

# Now the tricky part - we touch foo.h (which has refs to both .o files) and
# see if only foo.c is re-compiled. Also, the foo.h->bar.o link should be gone
rm foo.o bar.o
tup touch foo.h
tup upd
sym_check foo.o foo1 x
check_not_exist bar.o
tup_dep_no_exist foo.h bar.o

# Make sure the foo.h->foo.c still exists and wasn't marked obsolete for some
# reason.
tup touch foo.h
tup upd
tup_dep_exist foo.h foo.o
