#! /bin/sh -e

# The monitor changes its working directory to the directory of a file
# modification.  In this test, we get the monitor to have its current directory
# set to a location that is then deleted. A future modification to another
# valid file should allow the autoupdate to work, instead of trying to run it
# from a deleted directory.

. ./tup.sh
check_monitor_supported
tup monitor --autoupdate

mkdir dir2
cat > dir2/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > dir2/ok.c

tup flush
sym_check dir2/ok.o foo

mkdir tmp
cat > tmp/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > tmp/ok.c
tup flush
sym_check tmp/ok.o foo

# Monitor now has CWD as tmp, so remove that directory.
rm -rf tmp
tup flush

# Make a change to the still existing file - the autoupdate should work
# successfully.
echo 'int bar(void) {return 7;}' > dir2/ok.c
tup flush
sym_check dir2/ok.o bar

eotup
