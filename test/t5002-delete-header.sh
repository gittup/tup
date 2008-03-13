#! /bin/sh

. ../tup.sh

(echo "#include \"foo.h\""; echo "void foo1(void) {}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "" > foo.h
tup touch foo.c bar.c foo.h
tup upd
check_empty_tupdirs
sym_check foo.o foo1
sym_check bar.o bar1
sym_check prog_ foo1 bar1

# When the header is removed, the files should be re-compiled. Note we aren't
# doing a 'tup touch foo.c' here
rm foo.h bar.o foo.o
tup delete foo.h
echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup upd
check_empty_tupdirs
sym_check foo.o foo1
sym_check bar.o bar1
