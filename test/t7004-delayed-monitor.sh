#! /bin/sh -e

. ../tup.sh

# First create a foo.c program, then stop the monitor
tup monitor
cp ../testMakefile Makefile
echo "int main(void) {return 0;}" > foo.c
update
tup stop

# Now we make a change outside of the monitor's control (create a new file)
echo "void bar(void) {}" > bar.c
tup monitor
update
tup_object_exist bar.c bar.o
sym_check prog bar
tup stop

# Now we make another change outside of the monitor's control (modify a file)
echo "void bar2(void) {}" >> bar.c
tup monitor
update
sym_check prog bar bar2
tup stop

# Finally, delete a file outside of the monitor's control
rm bar.c
tup monitor
update
sym_check prog ~bar ~bar2
tup stop
