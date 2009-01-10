#! /bin/sh -e

# Yet another broken update test - this time we have a rule that takes all .o
# files and outputs a .o. Normally this works fine since the output file is
# marked DELETE before the pattern match executes, so the output file won't be
# taken in as input. However, if the tup database is wiped out, and then we
# leave the output .o hanging around and try to do an update, we don't know
# that the command outputs the file and so it won't be ignored. This test
# verifies that that particular bug is fixed.
#
# Fixing this is nice because it is an easy-to-detect circular dependency,
# which currently can be annoying to remove by hand.
. ../tup.sh
cat > Tupfile << HERE
: foreach *.c >> gcc -c %f -o %F.o >> %F.o
: *.o >> ld -r %f -o built-in.o >> built-in.o
HERE

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main
sym_check built-in.o main

rm -rf .tup
tup init
tup touch foo.c foo.o built-in.o Tupfile
tup upd
