#! /bin/sh -e

# Make sure a ghost variable gets removed when necessary.

. ../tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(CONFIG_BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %F.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_exist @ CONFIG_BAR

cat > Tupfile << HERE
file-y = foo.c
: foreach \$(file-y) |> cat %f > %o |> %F.o
HERE
tup touch Tupfile
update

tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_no_exist @ CONFIG_BAR
