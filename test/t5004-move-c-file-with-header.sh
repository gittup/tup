#! /bin/sh -e

. ../tup.sh

# Verify both files are compiled
(echo "#include \"foo.h\""; echo "void foo1(void) {}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "#define FOO 3" > foo.h
tup touch foo.c bar.c foo.h
update
sym_check foo.o foo1
sym_check bar.o bar1
sym_check prog_ foo1 bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup delete bar.c
tup create realbar.c
update
check_not_exist bar.o
tup_object_no_exist bar.c bar.o
sym_check realbar.o bar1
sym_check prog_ foo1 bar1

# foo.h now has a dangling ref to bar.o - try to update foo.h
tup_dep_exist foo.h bar.o
tup touch foo.h
update
tup_dep_no_exist foo.h bar.o
