#! /bin/sh -e

# Make sure that when we remove a directory from a Tupfile, that directory
# no longer points to the Tupfile's directory.

. ./tup.sh
tmkdir subdir
cat > Tupfile << HERE
: subdir/*.c |> echo %f |>
HERE
tup touch Tupfile subdir/a.c
update
tup_dep_exist . subdir 0 .

rm Tupfile
tup rm Tupfile
update
tup_dep_no_exist . subdir 0 .

eotup
