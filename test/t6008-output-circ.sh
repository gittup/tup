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
#
# Actually, I've now changed the behavior to yell at you. So the user has the
# option of either removing the offending file, or fixing the Tupfile. We test
# both cases here.
. ../tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %F.o |> %F.o
: *.o |> ld -r %f -o built-in.o |> built-in.o
HERE

echo "int main(void) {return 0;}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o main
sym_check built-in.o main

rm -rf .tup
tup init
tup touch foo.c foo.o built-in.o Tupfile
update_fail

# First try: remove the offending files and update again
rm foo.o built-in.o
tup delete foo.o built-in.o
update

# Go back to our error scenario
rm -rf .tup
tup init
tup touch foo.c foo.o built-in.o Tupfile
update_fail

# Second try: remove only foo.o for now, we'll just deal with built-in.o
rm foo.o
tup delete foo.o
update_fail

# Now we assume built-in.o is actually a user file, and then change our command
# to write to new-built-in.o We want to make sure built-in.o still exists, and
# wasn't mysteriously deleted by tup. I modify foo.c here so the linker
# doesn't whine about multiple main definitions (from foo.o and the old
# built-in.o).
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %F.o |> %F.o
: *.o |> ld -r %f -o new-built-in.o |> new-built-in.o
HERE
echo "int foo(void) {return 0;}" > foo.c
tup touch Tupfile foo.c
update

check_exist foo.o built-in.o new-built-in.o
