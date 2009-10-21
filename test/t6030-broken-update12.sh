#! /bin/sh -e

# This broke in commit d404764edf235a8e7aaa9d4cfdb1055bcd791aa6. I was using
# a per-Tupfile delete tree, which doesn't work when a dependent subdirectory
# uses files that may be deleted. It worked in the earlier delete_tree removal
# because I was deleting files immediately. I still like deleting files in a
# separate "phase", but there needs to be a global cache of which files that
# will be.
#
# This test just mimics how it was discovered.

. ../tup.sh
tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cat > Tupfile << HERE
: sub/*.o |> ar rcs %o %f |> libfoo.a
HERE
tup touch sub/foo.c sub/bar.c sub/Tupfile Tupfile
update

rm sub/bar.c
tup rm sub/bar.c
update
