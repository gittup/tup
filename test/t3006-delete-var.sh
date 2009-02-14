#! /bin/sh -e

# Verify that removing a variable from the database will cause a dependent
# Tupfile to be re-parsed.

. ../tup.sh
cat > Tupfile << HERE
src-y:=
src-@BLAH@ = foo.c
: foreach \$(src-y) |> echo gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c
tup varset BLAH sup
update

tup kconfig_pre_delete
tup kconfig_post_delete

# Update should now fail because @BLAH@ is undefined in the Tupfile.
update_fail
