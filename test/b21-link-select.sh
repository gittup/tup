#! /bin/sh -e
# First create all the links from before, then see if we can find all of them.
# Also verify at least the first and last links actually exist in the db by
# checking the return code.
. ../b11-link.sh

for i in `seq 1 $1`; do tup link_exists . $i . foo; tup link_exists . foo . $i.o; done
if tup link_exists . 1 . foo; then
	:
else
	echo "Link from 1 -> foo doesn't exist!" 1>&2
	exit 1
fi

if tup link_exists . foo . $1.o; then
	:
else
	echo "Link from foo -> $1.o doesn't exist!" 1>&2
	exit 1
fi
