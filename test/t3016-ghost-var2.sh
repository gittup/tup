#! /bin/sh -e

# Make sure ghost variables stay when setting all normal variables.

. ../tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(GHOST) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %F.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
varsetall FOO=3
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_exist @ GHOST
tup_object_exist @ FOO
tup_dep_exist @ GHOST 0 .

# The GHOST variable should still exist and point to the directory
varsetall FOO=4
tup_object_exist @ GHOST
tup_dep_exist @ GHOST 0 .
