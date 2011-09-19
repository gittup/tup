#! /bin/sh -e

# Verify the --verbose flag.

. ./tup.sh

tmp=".tmp.txt"
cat > Tupfile << HERE
: ok.c |> ^ short^ gcc -c %f -o %o |> %B.o
HERE
tup touch ok.c Tupfile
update > $tmp

if ! grep short $tmp > /dev/null; then
	echo "Error: Expected 'short' to be in the output text." 1>&2
	exit 1
fi
if grep 'gcc -c ok.c -o ok.o' $tmp > /dev/null; then
	echo "Error: Expected the gcc string to be absent from the output text." 1>&2
	exit 1
fi

tup touch ok.c
update --verbose > $tmp
if ! grep short $tmp > /dev/null; then
	echo "Error: Expected 'short' to be in the output text." 1>&2
	exit 1
fi
if ! grep 'gcc -c ok.c -o ok.o' $tmp > /dev/null; then
	echo "Error: Expected the gcc string to be in the output text." 1>&2
	exit 1
fi

eotup
