#! /bin/sh -e

# See what happens if we use a variable that doesn't exist.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"

varsetall BAR=y
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_exist . "cat bar.c > bar.o"

eotup
