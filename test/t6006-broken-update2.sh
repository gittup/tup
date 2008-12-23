#! /bin/sh -e

# Test for a specific bug - if we run an update and a command fails, then when
# we run update again it should start at that command again (or really any
# other available command). What it shouldn't do is assume everything is up-to-
# date.
. ../tup.sh
cp ../testTupfile Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main

echo "int main(void) {bork}" > foo.c
tup touch foo.c
update_fail

# Update again should fail again
update_fail
