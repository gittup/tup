#! /bin/sh -e
# First create all the links from before, then see if we can find all of them.
# Also verify at least the first and last links actually exist in the db by
# checking the return code.
. ../b11-link.sh

for i in $nums; do tup link_exists $i foo; tup link_exists foo $i.o; done
if tup link_exists 1 foo; then
	:
else
	exit 1
fi

if tup link_exists foo $1.o; then
	:
else
	exit 1
fi
