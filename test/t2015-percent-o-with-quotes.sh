#! /bin/sh -e

# Stumbled on this while trying to build busybox.

. ../tup.sh
cat > Tupfile << HERE
: foo.c |> sh -c "gcc -c %f -o %o" |> %B.o
HERE
tup touch foo.c Tupfile
tup parse
tup_object_exist . foo.c
tup_object_exist . "sh -c \"gcc -c foo.c -o foo.o\""
