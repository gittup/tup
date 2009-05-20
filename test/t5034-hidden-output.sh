#! /bin/sh -e

# Make sure we can write to hidden output files without tup breaking (it should
# display a warning message).

. ../tup.sh
cat > Tupfile << HERE
: |> echo foo > .hidden |>
HERE
tup touch Tupfile
tup upd
