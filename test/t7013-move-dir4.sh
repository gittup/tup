#! /bin/sh -e

# Make sure we can move a directory to a different place in the hierarchy. This
# means we have to update the node's dir field when it is renamed.
. ./tup.sh
tup monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
update

mv a/a2 .
update
check_exist a2/foo.o a2/prog

eotup
