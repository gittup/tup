#! /bin/sh -e

# See if we can escape an at-sign

. ../tup.sh
cat > Tupfile << HERE
: |> echo "\@hey" |>
HERE
tup touch Tupfile
tup upd

tup_object_exist . 'echo "@hey"'
