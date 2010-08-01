#! /bin/sh -e

# Make sure hidden files are still tracked internally during command
# execution, even though they won't make it into the final DAG.
. ./tup.sh

cat > Tupfile << HERE
: |> touch .foo; mv .foo bar |> bar
HERE
tup touch Tupfile
update

eotup
