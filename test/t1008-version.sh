#! /bin/sh -e

# Make sure 'tup version', 'tup -v', and 'tup --version' all do the same thing.
. ./tup.sh

a=`tup version`
b=`tup -v`
c=`tup --version`
if [ "$a" != "$b" ]; then
	echo "Error: Versions(a, b) don't match." 1>&2
	exit 1
fi
if [ "$a" != "$c" ]; then
	echo "Error: Versions(a, c) don't match." 1>&2
	exit 1
fi

eotup
