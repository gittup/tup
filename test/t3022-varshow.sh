#! /bin/sh -e

# Make sure 'varshow' works

. ../tup.sh
varsetall FOO=sup
tup read
tup varshow FOO
if tup varshow FOO | grep sup; then
	:
else
	echo "Variable not displayed in 'varshow'" 1>&2
	exit 1
fi
