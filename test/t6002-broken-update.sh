#! /bin/sh -e

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main

echo "int main(void) {bork}" > foo.c
tup touch foo.c
update_fail

echo "int main(void) {}" > foo.c
tup touch foo.c
if tup upd  | grep 'gcc.*foo.o' | wc -l | grep 2 > /dev/null; then
	:
else
	echo "foo.c should have been compiled and linked again." 1>&2
	exit 1
fi

eotup
