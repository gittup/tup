#! /bin/sh -e

# Make sure we can have an empty newline at the end of a Tupfile

. ../tup.sh
cat > Tupfile << HERE
: |> echo foo |>

HERE
tup touch Tupfile
update
