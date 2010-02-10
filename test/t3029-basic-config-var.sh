#! /bin/sh -e

# Same as t3000, only use the $(CONFIG_VAR) syntax.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-\$(CONFIG_BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
varsetall BAR=n
update
tup_object_exist . foo.c bar.c
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
vardict_exist BAR=n

varsetall BAR=y
update
tup_object_exist . foo.c bar.c
tup_object_exist . "cat foo.c > foo.o"
tup_object_exist . "cat bar.c > bar.o"
vardict_exist BAR=y

eotup
