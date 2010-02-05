#! /bin/sh -e

# This test checks for the issue where foreach on a different directory
# produced commands attached to the wrong dir (specifically, in the
# subdirectory rather than the parent directory).
. ./tup.sh

cat > Tupfile << HERE
: foreach input/*.o |> cat %f > %o |> %b
HERE

tmkdir input
cat > input/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "void foo(void) {}" > input/foo.c
echo "void bar(void) {}" > input/bar.c

tup touch Tupfile input/Tupfile input/foo.c input/bar.c
update

eotup
