#! /bin/sh

. ../tup.sh

# Verify both files are compiled
echo "void foo1(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
tup upd
check_empty_tupdirs
object_check foo.o foo1
object_check bar.o bar1

# Verify only foo is compiled if foo.c is touched
echo "void foo2(void) {}" >> foo.c
rm foo.o bar.o
tup touch foo.c
tup upd
check_empty_tupdirs
check_not_exist bar.o
object_check foo.o foo1 foo2

# Verify both are compiled if both are touched, but only linked once
rm foo.o
tup touch foo.c bar.c
if tup upd | grep LD | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Program should have only been linked once." 1>&2
fi
check_empty_tupdirs
object_check foo.o foo1 foo2
object_check bar.o bar1
