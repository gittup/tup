#! /bin/sh -e

# TODO
. ./tup.sh

cat > Tupfile << HERE
: |> touch .foo; mv .foo bar |> bar
HERE
tup touch Tupfile
update

eotup
