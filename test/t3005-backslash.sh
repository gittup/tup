#! /bin/sh -e

# Test backslash for line-continuation.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c \\
	 bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
update
tup_object_exist . foo.c bar.c
tup_object_exist . "cat foo.c > foo.o"
tup_object_exist . "cat bar.c > bar.o"

eotup
