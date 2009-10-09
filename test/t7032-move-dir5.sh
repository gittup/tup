#! /bin/sh -e

# This broke in commit 9df4109c16ac2c8075e73a8234d58a81e2cff44a - apparently I
# was relying on the watch to move when a directory moves.

. ../tup.sh
tup monitor
mkdir a
touch a/foo.c
tup flush
mv a b
update

echo ': foreach *.c |> gcc -c %f -o %o |> %B.o' > b/Tupfile
update
tup_object_exist . b
tup_object_exist b foo.o
tup_object_no_exist . a
check_exist b/foo.o
