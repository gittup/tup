#! /bin/sh -e

# Check that if a Tupfile stops using a variable, that var no longer has a
# dependency on the directory.

. ../tup.sh
tmkdir tmp
cat > tmp/Tupfile << HERE
file-y = foo.c
file-@(BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > tmp/foo.c
echo yo > tmp/bar.c
tup touch tmp/foo.c tmp/bar.c tmp/Tupfile
varsetall BAR=y
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "cat foo.c > foo.o"
tup_object_exist tmp "cat bar.c > bar.o"
tup_dep_exist @ BAR . tmp

cat > tmp/Tupfile << HERE
file-y = foo.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
tup touch tmp/Tupfile
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "cat foo.c > foo.o"
tup_object_no_exist tmp "cat bar.c > bar.o"
tup_dep_no_exist @ BAR . tmp
