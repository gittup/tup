#! /bin/sh -e

# There was a problem where the tup entry list was being taken and not returned
# if the command had an error while processing its inputs (for example, if it
# read from a generated file and the dependency was missing). In this case, a
# second command that was successful would be unable to update its links. This
# test reproduces that by using the -k flag to keep going after the first error
# (it could also happen randomly when parallelized).

. ./tup.sh

cat > Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

cat > foo.c << HERE
#include "foo.h"
HERE

tup touch Tupfile foo.c zap.c
update_fail -k

if tup upd -k 2>/dev/null | grep gcc | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Error: Only one file should have been compiled." 1>&2
	exit 1
fi

eotup
