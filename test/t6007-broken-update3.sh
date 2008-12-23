#! /bin/sh -e

# Another broken update test - this time we pretend to modify the Tupfile, so
# it will be parsed again and the commands re-generated. This is to test for
# a specific bug, namely that when an update fails and the command is still
# marked MODIFY, changing the Tupfile will re-create the command, and end up
# setting the command's flags to NONE, so it doesn't get executed. Whoops.
. ../tup.sh
cp ../testTupfile Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main

echo "int main(void) {bork}" > foo.c
tup touch foo.c
update_fail

tup touch Tupfile
if tup upd 2>&1 | grep 'gcc -c foo.c -o foo.o' > /dev/null; then
	:
else
	echo "foo.c should have been compiled again." 1>&2
	exit 1
fi
