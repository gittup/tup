#! /bin/sh -e

. ../tup.sh

# Make sure we don't get a warning for deleting a file if it was never created
# in the first place.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
tup scan
tup parse
if tup scan 2>&1 | grep 'tup warning'; then
	echo "Received warning text from tup scan." 1>&2
	exit 1
fi

tup upd
rm -f foo.o

if tup scan 2>&1 | grep 'tup warning'; then
	echo "Above warning correctly received."
else
	echo "Did not receive warning from tup scan" 1>&2
	exit 1
fi
