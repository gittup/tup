#! /bin/sh -e

# Apparently reading from the full path in a subdirectory is broken.

. ./tup.sh
tmkdir atmp
cat > atmp/Tupfile << HERE
: |> cat $PWD/atmp/foo.txt |>
HERE
tup touch atmp/foo.txt atmp/Tupfile
update

eotup
