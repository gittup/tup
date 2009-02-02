#! /bin/sh -e

# Try order-only-ish prerequisites. I use a command designed to break in order
# to stop before the 'echo' commands run since executing the commands would
# change the dependencies.

. ../tup.sh
cat > Tupfile << HERE
: |> borkborkborkmarf |> foo.h
: foreach *.c | foo.h |> echo gcc -c %f -o %o |> %F.o
HERE

tup touch Tupfile foo.h foo.c bar.c
update_fail
tup_dep_exist . foo.h . "echo gcc -c foo.c -o foo.o"
tup_dep_exist . foo.h . "echo gcc -c bar.c -o bar.o"
