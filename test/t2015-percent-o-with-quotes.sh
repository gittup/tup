#! /bin/sh -e

# Stumbled on this while trying to build busybox.

. ../tup.sh
cat > Tupfile << HERE
: foo.c |> echo "gcc -c %f -o %o" |> %B.o
HERE
tup touch foo.c Tupfile
update
tup_object_exist . foo.c
tup_object_exist . "echo \"gcc -c foo.c -o foo.o\""
