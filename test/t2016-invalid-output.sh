#! /bin/sh -e

# Verify that if we try to write outside of the directory where the Tupfile is,
# things explode.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c foo.c -o foo.o |> bar/foo.o
HERE
tmkdir bar
tup touch bar/hey
touch foo.c
tup touch foo.c Tupfile
update_fail

eotup
