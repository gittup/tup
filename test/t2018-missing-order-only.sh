#! /bin/sh -e

# Like t2011, but this time the order-only prerequisite is missing.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c | foo.h |> echo gcc -c %f -o %o |> %B.o
HERE

tup touch Tupfile foo.c bar.c
parse_fail_msg "Explicitly named file 'foo.h' not found"

eotup
