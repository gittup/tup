#! /bin/sh -e

. ./tup.sh
cp ../testTupfile.tup Tupfile

# Since the 'foreach *.c' in the Tupfile will process the files in alphabetical
# order, these files should be built in the order (bar.c, foo.c, zap.c). Then
# we make the middle file break in order to test the keep-going logic.

single_threaded
echo "void bar(void) {}" > bar.c
echo "int main(void) {bork; return 0;}" > foo.c
echo "void zap(void) {}" > zap.c
tup touch bar.c foo.c zap.c
update_fail

check_exist bar.o
check_not_exist foo.o zap.o prog

update_fail -k
check_exist bar.o zap.o
check_not_exist foo.o prog

eotup
