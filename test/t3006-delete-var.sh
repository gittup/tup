#! /bin/sh -e

# Verify that removing a variable from the database will cause a dependent
# Tupfile to be re-parsed.

. ../tup.sh
cat > Tupfile << HERE
src-y:=
src-@(BLAH) = foo.c
: foreach \$(src-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c
tup varset BLAH=y
update
tup_object_exist . "gcc -c foo.c -o foo.o"
check_exist foo.o

tup varset

# BLAH isn't set, so it is treated as empty
update
tup_object_no_exist . "gcc -c foo.c -o foo.o"
check_not_exist foo.o
