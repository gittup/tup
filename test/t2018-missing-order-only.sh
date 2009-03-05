#! /bin/sh -e

# Like t2011, but this time the order-only prerequisite is missing.

. ../tup.sh
cat > Tupfile << HERE
: foreach *.c | foo.h |> echo gcc -c %f -o %o |> %F.o
HERE

tup touch Tupfile foo.c bar.c
if tup parse; then
	echo "Error: Shouldn't be able to parse a non-existant order-only prereq." 1>&2
	exit 1
else
	echo "Hooray, parsing correctly failed."
fi
