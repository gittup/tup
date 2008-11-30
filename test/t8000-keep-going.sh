#! /bin/sh -e

. ../tup.sh
cp ../testTupfile Tupfile

# Admittedly, I don't really know how the order these files are created
# determines the order in which they are processed in the updater. I just
# ran it and saw the order they go in (should be bar.c, foo.c, foo2.c) and then
# made the middle one (foo.c) break. Seemingly innocuous changes to the updater
# could break this test. If so, they could just be re-ordered, or I could
# figure out how it's supposed to be done. Obviously, these files can't be
# made dependent on each other, since we're supposed to test the keep_going
# logic, which requires independent commands.

echo "void bar(void) {}" > bar.c
echo "int main(void) {bork; return 0;}" > foo.c
echo "void foo2(void) {}" > foo2.c
tup touch foo.c foo2.c bar.c
update_fail

check_exist bar.o
check_not_exist foo2.o foo.o prog

update_fail -k
check_exist bar.o foo2.o
check_not_exist foo.o prog
